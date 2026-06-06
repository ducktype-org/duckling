
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/safe/exceptions.hpp>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(deadlockTest);
		TESTER_ADD_TEST(deadlockWakingFromCv);
		TESTER_ADD_TEST(detectionDisabledAllowsNormalMutexUse);
	}

private:
	TestResult runWithDetection(const std::string& filename) {
		auto pid = initProcess(true);
		ASSERT_TRUE(vm::api::loadFiles(pid, { fs::File(path(filename)) }).has_value());
		return runTestOnVmGetResult(pid);
	}

	void deadlockTest() {
        // Two threads wait on a barrier and when released they deadlock. Main waits for them by joining.
		assertExecutionPanickedWith(
			runWithDetection("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runWithDetection("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runWithDetection("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runWithDetection("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runWithDetection("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
    }

    void deadlockWakingFromCv() {
        // One thread sleeps on CV while main and other thread deadlock
		assertExecutionPanickedWith(
			runWithDetection("cv_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
		// Two threads deadlock and wake main that will finish execution
		assertExecutionPanickedWith(
			runWithDetection("cv_main_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
	}

	void detectionDisabledAllowsNormalMutexUse() {
		// Spawn a process with deadlock detection disabled (the default) and verify normal mutex usage still works.
		auto pid  = initProcess(false);
		auto file = fs::File(path("safe_mutex.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		handleTestResult(runTestOnVmGetResult(pid), 0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/deadlock_detection/");
