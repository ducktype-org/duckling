
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(deadlockTest);
		TESTER_ADD_TEST(deadlockWakingFromCv);
	}

private:
	void deadlockTest() {
        // Two threads wait on a barrier and when released they deadlock. Main waits for them by joining.
		assertExecutionPanickedWith(
			runTestOnVmGetResult("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runTestOnVmGetResult("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runTestOnVmGetResult("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runTestOnVmGetResult("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
        assertExecutionPanickedWith(
			runTestOnVmGetResult("deterministic_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
    }
    void deadlockWakingFromCv() {
        // One thread sleeps on CV while main and other thread deadlock
		assertExecutionPanickedWith(
			runTestOnVmGetResult("cv_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
		// Two threads deadlock and wake main that will finish execution
		assertExecutionPanickedWith(
			runTestOnVmGetResult("cv_main_deadlock.dbc"),
			vm::exceptions::VMDeadlockException::ERR_MSG
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/deadlock_detection/");
