#include "simple_debug_info.hpp"

#include <debug_info/debug_info_io.hpp>

#include <base/comptime/type_traits.hpp>

#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <variant>

#define altIndex(t) base::variantTypeIndex<vm::api::ProcStatus, t>()

class VmDebuggerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(getStatusWait);
		TESTER_ADD_TEST(continuePauseTest);
		TESTER_ADD_TEST(noRunTest);
		TESTER_ADD_TEST(runAndGetStatus);
		TESTER_ADD_TEST(getStatusBreakpoint);
		TESTER_ADD_TEST(rerunTest);
		TESTER_ADD_TEST(errorTest);
		TESTER_ADD_TEST(outputTest);
		TESTER_ADD_TEST(memoryTest);
		TESTER_ADD_TEST(inputTest);
		TESTER_ADD_TEST(loadedFilesTest);
		TESTER_ADD_TEST(loadDefaultTest);
	}

private:
	void loadedFilesTest() {
		vm::debugger::Debugger debugger;
		auto                   loaded = fs::File(path("debugger_test.dbc"));
		auto                   other  = fs::File(path("while_true.dbc"));

		ASSERT_TRUE(debugger.getLoadedFiles().empty());
		ASSERT_TRUE(!debugger.isFileAvailable(loaded));

		ASSERT_HAS_VALUE(debugger.loadFiles({ loaded }));

		ASSERT_EQUAL_PRINT(debugger.getLoadedFiles().size(), usize(1));
		ASSERT_TRUE(debugger.getLoadedFiles().contains(loaded));
		ASSERT_TRUE(debugger.isFileAvailable(loaded));
		ASSERT_TRUE(!debugger.isFileAvailable(other));
	}

	void loadDefaultTest() {
		fs::FilePath package = fs::FileManager::createRandomTempDirectory().getFilePath();
		fs::FilePath build   = package / "duck_build";

		vm::debugger::Debugger debugger;
		ASSERT_TRUE(!debugger.loadDefault(package).has_value());

		std::filesystem::create_directories(build.getPath());
		std::filesystem::copy_file(path("debugger_test.dbc"), (build / "package_dvm.dbc").getPath());
		ASSERT_TRUE(!debugger.loadDefault(package).has_value());
		ASSERT_TRUE(debugger.getLoadedFiles().empty());

		// Real mappings contain absolute paths
		auto          source = fs::File(path("simple.dk"));
		std::ofstream out(build.getPath() / "package_dvm.di", std::ios::binary);
		debug_info::saveToStream(
			debugger_test_data::simpleDebugInfo(source.getFilePath().string()), out
		);
		out.close();

		ASSERT_HAS_VALUE(debugger.loadDefault(package));

		ASSERT_EQUAL_PRINT(debugger.getLoadedFiles().size(), usize(1));
		ASSERT_TRUE(debugger.isFileAvailable(source));
		ASSERT_TRUE(!debugger.isFileAvailable(fs::File(path("while_true.dbc"))));

		fs::FileManager::deleteFolder(package, true);
	}

	void noRunTest() {
		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("debugger_test.dbc")) }));
		ASSERT_MATCHES(debugger.getStatus(), vm::api::NotStarted);
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
		std::atomic<usize>      status_counter   = 0;
		std::atomic<usize>      ret_val_counter  = 0;
		std::atomic<usize>      position_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path(std::string(path_name))) }));

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			bool notify = false;
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				variant_match(status) {
					variant_case(vm::api::ExecutionCompleted, completed) {
						auto exit_value_variant = completed.exit_value;
						ASSERT_MATCHES(exit_value_variant, std::vector<Ref<vm::IVMValue>>);
						auto exit_value = v_get(exit_value_variant, std::vector<Ref<vm::IVMValue>>);

						ASSERT_TRUE(ret_val_counter < expected_values.size());
						ASSERT_EQUAL_PRINT(1, exit_value.size());
						ASSERT_EQUAL_PRINT("i64", exit_value[0]->getType()->getName());
						ASSERT_EQUAL_PRINT(
							expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
						);
						ret_val_counter++;
					}
					variant_case(vm::api::Paused, paused) {
						auto code_pos = debugger.getCurrentPosition();
						ASSERT_HAS_VALUE(code_pos);
						ASSERT_TRUE(position_counter < breakpoints.size());
						ASSERT_HAS_VALUE(code_pos.value().source_position);
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
			ASSERT_HAS_VALUE(
				debugger.setBreakpoint(fs::File(path(std::string(path_name))), breakpoint)
			);
		ASSERT_HAS_VALUE(debugger.runMain());
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
			"io_hang.dbc",
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
			{ 13, 19 }
		);
	}

	void continuePauseTest() {
		std::atomic<usize> status_counter = 0;

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

		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("while_true.dbc")) }));
		ASSERT_HAS_VALUE(debugger.setBreakpoint(base::StrID("main"), 0));

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);

		ASSERT_HAS_VALUE(debugger.runMain());

		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return v_matches(debugger.getStatus(), vm::api::Paused);
			}));
		}

		ASSERT_HAS_VALUE(debugger.resume());
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return v_matches(debugger.getStatus(), vm::api::Running);
			}));
		}

		ASSERT_HAS_VALUE(debugger.pause());
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return v_matches(debugger.getStatus(), vm::api::Paused);
			}));
		}

		ASSERT_HAS_VALUE(debugger.resume());
		{
			std::unique_lock lk(m);
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return v_matches(debugger.getStatus(), vm::api::Running);
			}));
		}

		ASSERT_HAS_VALUE(debugger.pause());
		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return status_counter == expected_statuses.size();
		}));

		ASSERT_MATCHES(debugger.getStatus(), vm::api::Paused);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter.load());
	}

	void rerunTest() {
		std::atomic<usize> status_counter  = 0;
		std::atomic<usize> ret_val_counter = 0;

		std::mutex              m;
		std::condition_variable cv;

		// A re-run of a terminal process first resets it, which is a real, emitted state
		// change - so from the second run on the sequence starts with `NotStarted`.
		// `expected_statuses` is modified by the main thread and should be read under `m`.
		std::vector<usize> expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::ExecutionCompleted),
		};

		const std::vector<i32> expected_values = { 0, 0, 0 };

		std::atomic<bool> all_expected_seen = false;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			{
				std::lock_guard lk(m);
				ASSERT_TRUE(status_counter < expected_statuses.size());
				ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
				variant_match(status) {
					variant_case(vm::api::ExecutionCompleted, completed) {
						auto exit_value_variant = completed.exit_value;
						ASSERT_MATCHES(exit_value_variant, std::vector<Ref<vm::IVMValue>>);
						auto exit_value = v_get(exit_value_variant, std::vector<Ref<vm::IVMValue>>);

						ASSERT_TRUE(ret_val_counter < expected_values.size());
						ASSERT_EQUAL_PRINT(1, exit_value.size());
						ASSERT_EQUAL_PRINT("i64", exit_value[0]->getType()->getName());
						ASSERT_EQUAL_PRINT(
							expected_values[ret_val_counter], exit_value[0]->readBytes<i64>()
						);
						ret_val_counter++;
					}
				}
				status_counter++;
				all_expected_seen = status_counter == expected_statuses.size();
			}
			if (all_expected_seen) cv.notify_one();
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("debugger_test.dbc")) }));

		debugger.attachOnStatusChangedListener(status_listener);
		debugger.attachOnErrorListener(error_listener);

		i32  loop      = 3;
		bool first_run = true;

		while (loop-- > 0) {
			{
				std::lock_guard lk(m);
				status_counter    = 0;
				all_expected_seen = false;
				if (!first_run)
					expected_statuses = { altIndex(vm::api::NotStarted),
						                  altIndex(vm::api::Running),
						                  altIndex(vm::api::ExecutionCompleted) };
				first_run = false;
			}
			ASSERT_HAS_VALUE(debugger.runMain());
			std::unique_lock lk(m);
			// Test timeout
			ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return status_counter == expected_statuses.size();
			}));
			ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter.load());
		}
		ASSERT_MATCHES(debugger.getStatus(), vm::api::ExecutionCompleted);
		std::lock_guard lk(m);
		ASSERT_EQUAL_PRINT(expected_values.size(), ret_val_counter.load());
	}

	void errorTest() {
		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("while_true_no_breakpoint.dbc")) }));

		ASSERT_HAS_VALUE(debugger.runMain());
		ASSERT_TRUE(!debugger.runMain());  // 1st error

		ASSERT_TRUE(!debugger.resume());   // 2nd error

		ASSERT_TRUE(!debugger.step());     // 3rd error

		ASSERT_HAS_VALUE(debugger.pause());

		ASSERT_TRUE(!debugger.pause());  // 4rd error

		ASSERT_HAS_VALUE(debugger.resume());

		ASSERT_MATCHES(debugger.getStatus(), vm::api::Running);
	}

	/**
	 * @brief test if the output emitter works
	 */
	void outputTest() {
		std::atomic_bool        output = false;
		std::condition_variable cv;
		std::mutex              m;

		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("vm_api_tests.dbc")) }));

		events::Listener<std::string> output_listener([&](const std::string& str) {
			ASSERT_EQUAL_PRINT("7", str);
			output.store(true);
			cv.notify_all();
		});

		debugger.attachOnOutputListener(output_listener);

		ASSERT_HAS_VALUE(debugger.runMain());

		std::unique_lock lk(m);
		// Test timeout
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] { return output.load(); }));
	}

	void memoryTest() {
		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("breakpoint_all_types.dbc")) }));
		ASSERT_HAS_VALUE(debugger.setBreakpoint(base::StrID("main"), 20));
		std::mutex m;

		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			if (status.index() == altIndex(vm::api::Paused)) cv.notify_one();
		});
		debugger.attachOnStatusChangedListener(status_listener);
		ASSERT_HAS_VALUE(debugger.runMain());
		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return v_matches(debugger.getStatus(), vm::api::Paused);
		}));

		{
			// Code position Test
			auto pos_response = debugger.getCurrentPosition();
			ASSERT_HAS_VALUE(pos_response);
			auto code_position = pos_response.value();
			ASSERT_EQUAL_PRINT("main", code_position.function_name);
			ASSERT_EQUAL_PRINT(20, code_position.instr_number);
			ASSERT_HAS_VALUE(code_position.source_position);
		}


		auto main_thread_id = vm::api::ThreadID(0);

		auto response = debugger.getNumberOfStackFrames(main_thread_id).value();
		ASSERT_EQUAL(2, response);
		auto info = debugger.getStackFrameData(main_thread_id, 1).value();
		ASSERT_EQUAL(9, info.frame_vars.size());
		ASSERT_EQUAL_PRINT("0", info.frame_vars[0].value->str());          // ret0
		ASSERT_EQUAL_PRINT("0", info.frame_vars[1].value->str());          // arg0
		ASSERT_EQUAL_PRINT("null", info.frame_vars[2].value->str());       // arg1
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[3].value->str());  // struct_pointer
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[4].value->str());  // dyntable_pointer
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[5].value->str());  // fixtable_pointer
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[6].value->str());  // variant_pointer
		ASSERT_EQUAL_PRINT("<pointer>", info.frame_vars[7].value->str());  // variant_data_pointer
		ASSERT_EQUAL_PRINT("42", info.frame_vars[8].value->str());         // new_variant_data_value

		// Step test
		ASSERT_HAS_VALUE(debugger.step());
		{
			auto pos_response = debugger.getCurrentPosition();
			ASSERT_HAS_VALUE(pos_response);
			auto code_position = pos_response.value();
			ASSERT_EQUAL_PRINT("main", code_position.function_name);
			ASSERT_EQUAL_PRINT(21, code_position.instr_number);
			ASSERT_HAS_VALUE(code_position.source_position);
		}
	}

	void inputTest() {
		vm::debugger::Debugger debugger;
		ASSERT_HAS_VALUE(debugger.loadFiles({ fs::File(path("input.dbc")) }));
		std::mutex m;

		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			if (v_matches(status, vm::api::ExecutionCompleted)) cv.notify_one();
		});
		debugger.attachOnStatusChangedListener(status_listener);

		ASSERT_HAS_VALUE(debugger.runMain());

		ASSERT_HAS_VALUE(debugger.sendInput("2"));

		std::unique_lock lk(m);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return v_matches(debugger.getStatus(), vm::api::ExecutionCompleted);
		}));

		auto  status         = debugger.getStatus();
		auto  completed_info = std::get<vm::api::ExecutionCompleted>(status);
		auto& exit_value     = std::get<std::vector<Ref<vm::IVMValue>>>(completed_info.exit_value);
		auto  optional_data  = exit_value[0]->readData();

		ASSERT_HAS_VALUE(optional_data);

		auto data_variant   = optional_data.value();
		auto primitive_data = std::get<vm::interpreted_data_variant::Primitive>(data_variant);

		ASSERT_EQUAL_PRINT(primitive_data.value, 2);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
