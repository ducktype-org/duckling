#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#include <condition_variable>
#include <mutex>

#define altIndex(t) base::internal::alternativeIndex<vm::api::ProcStatus, t>()

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
		const std::vector<usize>& expected_statuses
	) {
		std::atomic<size_t>     status_counter  = 0;
		std::atomic<size_t>     ret_val_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			bool notify = false;
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				status_counter++;
				notify
					= (status_counter == expected_statuses.size()
				       && ret_val_counter == expected_values.size());
			}
			if (notify) cv.notify_one();
		});

		events::Listener<vm::api::ExitValue> execution_completed_listener(
			[&](const vm::api::ExitValue& exit_value) {
				bool notify = false;
				{
					std::lock_guard lk(m);
					ASSERT_TRUE(ret_val_counter < expected_values.size());
					ASSERT_EQUAL_PRINT(1, exit_value.size());
					ASSERT_EQUAL_PRINT("i64", exit_value[0]->type->getName());
					ASSERT_EQUAL_PRINT(
						expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
					);
					ret_val_counter++;
					notify
						= (status_counter == expected_statuses.size()
				           && ret_val_counter == expected_values.size());
				}
				if (notify) cv.notify_one();
			}
		);

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path(std::string(path_name))) };
		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnExecutionCompletedListener(execution_completed_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.runMain();
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
			}
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

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);

		debugger.runMain();

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
				status_counter++;
			}
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<vm::api::ExitValue> execution_completed_listener(
			[&](const vm::api::ExitValue& exit_value) {
				std::lock_guard lk(m);
				ASSERT_TRUE(ret_val_counter < expected_values.size());
				ASSERT_EQUAL_PRINT(1, exit_value.size());
				ASSERT_EQUAL_PRINT("i64", exit_value[0]->type->getName());
				ASSERT_EQUAL_PRINT(
					expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
				);
				ret_val_counter++;
			}
		);
		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path("debugger_test.dbc")) };

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnExecutionCompletedListener(execution_completed_listener);
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

		debugger.pause();
		ASSERT_TRUE(!debugger.pause());  // 3rd error

		debugger.resume();

		ASSERT_TRUE(std::holds_alternative<vm::api::Running>(debugger.getStatus()));
	}

	void memoryTest() {
		vm::debugger::Debugger debugger{ fs::File(path("breakpoint_all_types.dbc")) };
		std::mutex             m;

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
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
