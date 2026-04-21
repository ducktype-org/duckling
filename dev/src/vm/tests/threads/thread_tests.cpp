
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/core/process/exceptions.hpp>

#include <chrono>
#include <thread>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadingTest);
		TESTER_ADD_TEST(mutexTest);
		TESTER_ADD_TEST(cvTest);
		TESTER_ADD_TEST(selfDeadlockPanicsTest);
		TESTER_ADD_TEST(selfDeadlockPanicsStressTest);
		TESTER_ADD_TEST(deterministicDeadlockPanicsTest);
	}

private:
	void runDeadlockTestAndForceStop(std::string_view dbc_filename, bool force_kill = true) {
		auto pid = initProcess();

		auto load_result = vm::api::loadFiles(pid, { fs::File(path(std::string(dbc_filename))) });
		ASSERT_TRUE(load_result.has_value());

		auto run_result = vm::api::run(pid);
		ASSERT_TRUE(run_result.has_value());

		std::string panic_error_message;
		bool        deadlock_detected = false;

		for (usize i = 0; i < 2000; ++i) {
			auto status_result = vm::api::getExecutionStatus(pid);
			ASSERT_TRUE(status_result.has_value());

			if (std::holds_alternative<vm::api::ExecutionPanicked>(status_result.value())) {
				panic_error_message
					= std::get<vm::api::ExecutionPanicked>(status_result.value()).error_message;
				deadlock_detected = panic_error_message.contains("Deadlock detected");
				break;
			}

			if (vm::api::isStatusTerminal(status_result.value())) break;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		ASSERT_TRUE(deadlock_detected);

		// Forcefully terminate process right after deadlock detection.
		// This avoids blocking on joins in intentionally deadlocked scenarios.
		if (force_kill) {
			auto kill_result = vm::api::kill(pid);
			ASSERT_TRUE(kill_result.has_value());
		}
	}

	void multithreadingTest() {
		// We don't check result here - only if it finished sucessfully
		// That's because this code is purposefully not deterministic - it has data race
		runTestOnVm("multithreading.dbc", "", {}, {});
	}

	void mutexTest() {
		// On the contrary here, this test should give clear result
		for (usize i = 0; i < 5; i++) runTestOnVm("mutex.dbc", "", "20000", {});
	}

	void cvTest() {
		runTestOnVm("cv_permit_barrier_test.dbc", "", "20000221", {});
		runTestOnVm("cv_simple_barrier_all_test.dbc", "", "22020201", {});
		runTestOnVm("producer_consumer.dbc", "", "20000200000221", {});
	}

	void selfDeadlockPanicsTest() { runDeadlockTestAndForceStop("self_deadlock.dbc"); }

	void selfDeadlockPanicsStressTest() {
		for (usize i = 0; i < 5; i++) runDeadlockTestAndForceStop("self_deadlock.dbc");
	}

	void deterministicDeadlockPanicsTest() {
		runDeadlockTestAndForceStop("deterministic_deadlock.dbc", false);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
