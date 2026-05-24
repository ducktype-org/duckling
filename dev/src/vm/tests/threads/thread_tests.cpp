
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadingTest);
		TESTER_ADD_TEST(multithreadZeroDivI32Test);
		TESTER_ADD_TEST(multithreadZeroDivI64Test);
		TESTER_ADD_TEST(mainThreadZeroDivWithWorkerTest);
		TESTER_ADD_TEST(mutexTest);
		TESTER_ADD_TEST(cvTest);
		TESTER_ADD_TEST(reuseThreadTest);
	}

private:
	void multithreadingTest() {
		// We don't check result here - only if it finished sucessfully
		// That's because this code is purposefully not deterministic - it has data race
		runTestOnVm("multithreading.dbc", "", {}, {});
	}

	void multithreadZeroDivI32Test() {
		// Spawn a thread that performs division by zero on i32 and verify VM panicked
		auto result = runTestOnVmGetResult("multithread_zero_div.dbc");
		assertExecutionPanickedWith(result, vm::exceptions::VMZeroDivisionException::ERR_MSG);
	}

	void multithreadZeroDivI64Test() {
		// Spawn a thread that performs division by zero on i64 and verify VM panicked
		auto result = runTestOnVmGetResult("multithread_zero_div_i64.dbc");
		assertExecutionPanickedWith(result, vm::exceptions::VMZeroDivisionException::ERR_MSG);
	}

	void mainThreadZeroDivWithWorkerTest() {
		// Start a worker thread, let it run, then verify the main thread panics.
		auto result = runTestOnVmGetResult("main_thread_zero_div.dbc");
		assertExecutionPanickedWith(result, vm::exceptions::VMZeroDivisionException::ERR_MSG);
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

	void reuseThreadTest() { runTestOnVm("reuse_thread_test.dbc", "", "149501", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
