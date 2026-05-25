#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

class VmFastTrackTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFastTrackTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(fastTrackRaceTest);
		TESTER_ADD_TEST(fastTrackNoRaceTest);
		TESTER_ADD_TEST(fastTrackDisabledRaceTest);
		TESTER_ADD_TEST(extensiveNoRaceTest);
		TESTER_ADD_TEST(extensiveHeapRaceTest);
		TESTER_ADD_TEST(extensiveStackRaceTest);
	}

private:
	void fastTrackRaceTest() {
		// Enable FastTrack process settings
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = true;
		auto pid = initProcess(settings);

		// Load race_test.dbc which has a data race
		auto file = fs::File(path("race_test.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		// Running it should trigger a race and panic
		auto result = runTestOnVmGetResult(pid, "", {}, {});

		auto exec_status = vm::api::getExecutionStatus(pid);
		bool got_race_panic = false;
		if (exec_status.has_value()) {
			nlohmann::json status_json = exec_status.value();
			std::cout << "RACE TEST FINAL STATUS: " << status_json.dump() << std::endl;
			
			variant_match(exec_status.value()) {
				variant_case(vm::api::ExecutionPanicked, panicked) {
					got_race_panic = panicked.error_message.contains("Tried dividing by zero") ||
					                 panicked.error_message.contains("[FastTrack] Data race detected");
				}
				variant_default {}
			}
		}
		ASSERT_TRUE(got_race_panic);

		// Clean up the panicked process to prevent memory leaks and thread warnings
		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_TRUE(validation_result.has_value());
	}

	void fastTrackNoRaceTest() {
		// Enable FastTrack process settings
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = true;
		auto pid = initProcess(settings);

		// Load no_race_test.dbc which has no data race
		auto file = fs::File(path("no_race_test.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		// Running it should complete successfully
		runTestOnVm(pid, "", {}, {});
	}

	void fastTrackDisabledRaceTest() {
		// Keep FastTrack disabled
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = false;
		auto pid = initProcess(settings);

		// Load race_test.dbc which has a data race
		auto file = fs::File(path("race_test.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		// Running it should complete successfully (without throwing exception)
		auto result = runTestOnVmGetResult(pid, "", {}, {});
		if (!result.run_result.has_value()) {
			std::cout << "DISABLED RACE TEST FAILED to run: " << to_string(nlohmann::json(result.run_result.error())) << std::endl;
		} else {
			std::cout << "DISABLED RACE TEST completed. Exit code: " << result.run_result.value() << std::endl;
		}

		auto program_output = vm::api::output(pid);
		if (program_output.has_value()) {
			std::cout << "DISABLED RACE TEST OUTPUT: [" << program_output->output << "]" << std::endl;
		}

		// Validation
		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_TRUE(validation_result.has_value());
		ASSERT_TRUE(validation_result.value());
	}

	// ---------------------------------------------------------------
	// Extensive tests: heap/stack/global + nested structs + tables
	// ---------------------------------------------------------------

	/** All reads/writes happen AFTER join – FastTrack must NOT report a race. */
	void extensiveNoRaceTest() {
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = true;
		auto pid = initProcess(settings);

		auto file = fs::File(path("minimal_no_race.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		// Must complete without any panic
		runTestOnVm(pid, "", {}, {});
	}

	/** Main thread and child thread both write to the same nested heap struct
	 *  array element without synchronisation – FastTrack MUST detect a race. */
	void extensiveHeapRaceTest() {
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = true;
		auto pid = initProcess(settings);

		auto file = fs::File(path("extensive_heap_race.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		auto result = runTestOnVmGetResult(pid, "", {}, {});

		auto exec_status = vm::api::getExecutionStatus(pid);
		bool got_race_panic = false;
		if (exec_status.has_value()) {
			nlohmann::json status_json = exec_status.value();
			std::cout << "EXTENSIVE HEAP RACE STATUS: " << status_json.dump() << std::endl;

			variant_match(exec_status.value()) {
				variant_case(vm::api::ExecutionPanicked, panicked) {
					got_race_panic = panicked.error_message.contains("Tried dividing by zero") ||
					                 panicked.error_message.contains("[FastTrack] Data race detected");
				}
				variant_default {}
			}
		}
		ASSERT_TRUE(got_race_panic);

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_TRUE(validation_result.has_value());
	}

	/** Main thread and child thread both write to the same stack variable via a
	 *  shared pointer without synchronisation – FastTrack MUST detect a race. */
	void extensiveStackRaceTest() {
		vm::api::ProcessSettings settings;
		settings.enable_fast_track = true;
		auto pid = initProcess(settings);

		auto file = fs::File(path("extensive_stack_race.dbc"));
		auto load_res = vm::api::loadFiles(pid, { file });
		ASSERT_TRUE(load_res.has_value());

		auto result = runTestOnVmGetResult(pid, "", {}, {});

		auto exec_status = vm::api::getExecutionStatus(pid);
		bool got_race_panic = false;
		if (exec_status.has_value()) {
			nlohmann::json status_json = exec_status.value();
			std::cout << "EXTENSIVE STACK RACE STATUS: " << status_json.dump() << std::endl;

			variant_match(exec_status.value()) {
				variant_case(vm::api::ExecutionPanicked, panicked) {
					got_race_panic = panicked.error_message.contains("Tried dividing by zero") ||
					                 panicked.error_message.contains("[FastTrack] Data race detected");
				}
				variant_default {}
			}
		}
		ASSERT_TRUE(got_race_panic);

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_TRUE(validation_result.has_value());
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
