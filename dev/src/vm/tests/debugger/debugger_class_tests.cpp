#include <base/comptime/type_traits.hpp>

#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#include <condition_variable>
#include <mutex>

#define altIndex(t) base::variantTypeIndex<vm::api::ProcStatus, t>()

class VmDebuggerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO: #1222 Re-enable the tests after fixing the API.
		// TESTER_ADD_TEST(getStatusWait);
		// TESTER_ADD_TEST(continuePauseTest);

		TESTER_ADD_TEST(noRunTest);
		TESTER_ADD_TEST(runAndGetStatus);
		TESTER_ADD_TEST(getStatusBreakpoint);
		TESTER_ADD_TEST(rerunTest);
		TESTER_ADD_TEST(errorTest);
		TESTER_ADD_TEST(memoryTest);
		TESTER_ADD_TEST(inputTest);
	}

private:
	void noRunTest() {
		vm::debugger::Debugger debugger{ fs::File(path("debugger_test.dbc")) };
		ASSERT_TRUE(std::holds_alternative<vm::api::NotStarted>(debugger.getStatus()));
	}

	/**
	 * @brief template function to reuse in the tests
	 *
	 * @param path_name Path to dbc file that will be run
	 * @param expected_values Vector of expected return values
	 * @param expected_statuses Vector of expected status types (using altIndex(type))
	 */
	void testTemplate(
		std::string_view          path_name,
		const std::vector<int>&   expected_values,
		const std::vector<usize>& expected_statuses,
		const std::vector<usize>& breakpoints = {}
	) {
		std::atomic<size_t>     status_counter   = 0;
		std::atomic<size_t>     ret_val_counter  = 0;
		std::atomic<size_t>     position_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		vm::debugger::Debugger debugger{ fs::File(path(std::string(path_name))) };

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			bool notify = false;
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				variant_match(status) {
					variant_case(vm::api::ExecutionCompleted, completed) {
						auto exit_value = completed.exit_value;
						ASSERT_TRUE(ret_val_counter < expected_values.size());
						ASSERT_EQUAL_PRINT(1, exit_value.size());
						ASSERT_EQUAL_PRINT("i64", exit_value[0]->type->getName());
						ASSERT_EQUAL_PRINT(
							expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
						);
						ret_val_counter++;
					}
					variant_case(vm::api::Paused, paused) {
						auto code_pos = debugger.getCurrentPosition();
						ASSERT_TRUE(code_pos.has_value());
						ASSERT_TRUE(position_counter < breakpoints.size());
						ASSERT_TRUE(code_pos.value().source_position.has_value());
						auto line
							= code_pos.value().source_position.value().getStartLineColumn().first;
						ASSERT_EQUAL_PRINT(breakpoints[position_counter], line);
					}
				}
				status_counter++;
				notify = status_counter == expected_statuses.size();
			}
			if (notify) cv.notify_one();
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);
		for (u64 breakpoint: breakpoints)
			ASSERT_TRUE(debugger.setBreakpoint(fs::File(path(std::string(path_name))), breakpoint)
			                .has_value());
		ASSERT_TRUE(debugger.runMain().has_value());
		std::unique_lock lk(m);
		// timeout for the test
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return status_counter == expected_statuses.size();
		}));
		ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter.load());
		ASSERT_EQUAL_PRINT(expected_values.size(), ret_val_counter.load());
	}

	void runAndGetStatus() {
		testTemplate(
			"debugger_test.dbc",
			{ 0 },
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::ExecutionCompleted),
			}
		);
	}

	void getStatusWait() {
		testTemplate(
			"vm_api_tests.dbc",
			{},
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::Sleeping),
			}
		);
	}

	void getStatusBreakpoint() {
		testTemplate(
			"breakpoint.dbc",
			{},
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::Paused),
			},
			{ 13, 18 }
		);
	}

	void continuePauseTest() {
		std::atomic<size_t> status_counter = 0;

		std::mutex              m;
		std::condition_variable cv;

		const std::vector<usize> expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::Paused),  // breakpoint
			altIndex(vm::api::Running),
			altIndex(vm::api::Paused),  // pause 1
			altIndex(vm::api::Running),
			altIndex(vm::api::Paused),  // pause 2
		};

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				status_counter++;
			}
			if (status_counter > 1) cv.notify_one();
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path("while_true.dbc")) };
		ASSERT_TRUE(debugger.setBreakpoint(base::StrID("main"), 0).has_value());

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);

		ASSERT_TRUE(debugger.runMain().has_value());

		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return std::holds_alternative<vm::api::Paused>(debugger.getStatus());
			}));
		}

		debugger.resume();
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return std::holds_alternative<vm::api::Running>(debugger.getStatus());
			}));
		}

		debugger.pause();
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return std::holds_alternative<vm::api::Paused>(debugger.getStatus());
			}));
		}

		debugger.resume();
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return std::holds_alternative<vm::api::Running>(debugger.getStatus());
			}));
		}

		debugger.pause();
		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return status_counter == expected_statuses.size();
		}));

		ASSERT_TRUE(std::holds_alternative<vm::api::Paused>(debugger.getStatus()));
		ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter.load());
	}

	void rerunTest() {
		std::atomic<size_t> status_counter  = 0;
		std::atomic<size_t> ret_val_counter = 0;

		std::mutex              m;
		std::condition_variable cv;

		const std::vector<usize> expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::ExecutionCompleted),
		};

		const std::vector<int> expected_values = { 0, 0, 0 };

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				variant_match(status) {
					variant_case(vm::api::ExecutionCompleted, completed) {
						auto exit_value = completed.exit_value;
						ASSERT_TRUE(ret_val_counter < expected_values.size());
						ASSERT_EQUAL_PRINT(1, exit_value.size());
						ASSERT_EQUAL_PRINT("i64", exit_value[0]->type->getName());
						ASSERT_EQUAL_PRINT(
							expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
						);
						ret_val_counter++;
					}
				}
				status_counter++;
			}
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path("debugger_test.dbc")) };

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);

		int loop = 3;

		while (loop-- > 0) {
			status_counter = 0;
			debugger.runMain();
			std::unique_lock lk(m);
			// Test timeout
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return status_counter == expected_statuses.size();
			}));
			ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter.load());
		}
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(debugger.getStatus()));
		std::lock_guard lk(m);
		ASSERT_EQUAL_PRINT(expected_values.size(), ret_val_counter.load());
	}

	void errorTest() {
		vm::debugger::Debugger debugger{ fs::File(path("while_true_no_breakpoint.dbc")) };

		debugger.runMain();
		ASSERT_TRUE(!debugger.runMain());  // 1st error

		ASSERT_TRUE(!debugger.resume());   // 2nd error

		ASSERT_TRUE(!debugger.step());     // 3rd error

		ASSERT_TRUE(debugger.pause().has_value());

		ASSERT_TRUE(!debugger.pause());  // 4rd error

		ASSERT_TRUE(debugger.resume().has_value());

		ASSERT_TRUE(std::holds_alternative<vm::api::Running>(debugger.getStatus()));
	}

	void memoryTest() {
		vm::debugger::Debugger debugger{ fs::File(path("breakpoint_all_types.dbc")) };
		ASSERT_TRUE(debugger.setBreakpoint(base::StrID("main"), 20).has_value());
		std::mutex m;

		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			if (status.index() == altIndex(vm::api::Paused)) cv.notify_one();
		});
		debugger.attachOnStatusChangedListener(status_listener);
		debugger.runMain();
		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return std::holds_alternative<vm::api::Paused>(debugger.getStatus());
		}));

		{
			// Code position Test
			auto pos_response = debugger.getCurrentPosition();
			ASSERT_TRUE(pos_response.has_value());
			auto code_position = pos_response.value();
			ASSERT_EQUAL_PRINT("main", code_position.function_name);
			ASSERT_EQUAL_PRINT(20, code_position.instr_number);
			ASSERT_TRUE(code_position.source_position.has_value());
		}


		auto main_thread_id = vm::api::ThreadID(0);

		auto response = debugger.getNumberOfStackFrames(main_thread_id).value();
		ASSERT_EQUAL(2, response);
		auto info = debugger.getStackFrameData(main_thread_id, 1).value();
		ASSERT_EQUAL(9, info.frame_vars.size());
		ASSERT_EQUAL_PRINT("0", info.frame_vars[0].value.str());          // ret0
		ASSERT_EQUAL_PRINT("0", info.frame_vars[1].value.str());          // arg0
		ASSERT_EQUAL_PRINT("null", info.frame_vars[2].value.str());       // arg1
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[3].value.str());  // struct_pointer
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[4].value.str());  // dyntable_pointer
		ASSERT_EQUAL_PRINT(
			"<pointer>", info.frame_vars[5].value.str()
		);  // fixtable_pointer		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[6].value.str());
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[7].value.str());  // variant_pointer
		ASSERT_EQUAL_PRINT("42", info.frame_vars[8].value.str());         // new_variant_data_value

		// Step test
		ASSERT_TRUE(debugger.step().has_value());
		{
			auto pos_response = debugger.getCurrentPosition();
			ASSERT_TRUE(pos_response.has_value());
			auto code_position = pos_response.value();
			ASSERT_EQUAL_PRINT("main", code_position.function_name);
			ASSERT_EQUAL_PRINT(21, code_position.instr_number);
			ASSERT_TRUE(code_position.source_position.has_value());
		}
	}

	void inputTest() {
		vm::debugger::Debugger debugger{ fs::File(path("input.dbc")) };
		std::mutex             m;

		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			if (status.index() == altIndex(vm::api::ExecutionCompleted)) cv.notify_one();
		});
		debugger.attachOnStatusChangedListener(status_listener);

		debugger.runMain();

		debugger.input("2");

		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return std::holds_alternative<vm::api::ExecutionCompleted>(debugger.getStatus());
		}));

		auto status         = debugger.getStatus();
		auto completed_info = std::get<vm::api::ExecutionCompleted>(status);
		auto optional_data  = completed_info.exit_value[0]->readData();

		ASSERT_TRUE(optional_data.has_value());

		auto data_variant   = optional_data.value();
		auto primitive_data = std::get<vm::interpreted_data_variant::Primitive>(data_variant);

		ASSERT_EQUAL_PRINT(primitive_data.value, 2);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
