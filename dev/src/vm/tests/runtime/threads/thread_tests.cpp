// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmThreadTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadingTest);
		TESTER_ADD_TEST(mutexTest);
		TESTER_ADD_TEST(destroyLockedMutexTest);
		TESTER_ADD_TEST(multithreadZeroDiv);
		TESTER_ADD_TEST(cvTest);
		TESTER_ADD_TEST(reuseThreadTest);
	}

private:
	void multithreadingTest() {
		// We don't check result here - only if it finished successfully
		// That's because this code is purposefully not deterministic - it has data race
		runTestOnVm("multithreading.dbc", "", {}, {});
	}

	void multithreadZeroDiv() {
		// Spawn a thread that performs division by zero on i32 and verify VM panicked
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("multithread_zero_div.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		// Spawn a thread that performs division by zero on i64 and verify VM panicked
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("multithread_zero_div_i64.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		// Start a worker thread, let it run, then verify the main thread panics.
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("main_thread_zero_div.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		// Two threads in active zero-division with workers still alive
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("kill_threads_zero_division.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		// Two threads deadlock and main divides by zero. To be removed after introducing
		// deadlock detection.
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("deadlock_then_panic.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
	}

	void mutexTest() {
		// On the contrary here, this test should give clear result
		for (usize i = 0; i < 5; i++) runTestOnVm("mutex.dbc", "", "20000", {});
	}

	void destroyLockedMutexTest() {
		// Destroying a mutex that is still held is a program error the VM must report.
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("destroy_locked_mutex.dbc"),
			vm::exceptions::VMDestroyLockedMutexException::ERR_MSG
		);
	}

	void cvTest() {
		runTestOnVm("cv_permit_barrier_test.dbc", "", "20000221", {});
		runTestOnVm("cv_simple_barrier_all_test.dbc", "", "22020201", {});
		runTestOnVm("producer_consumer.dbc", "", "20000200000221", {});
	}

	void reuseThreadTest() { runTestOnVm("reuse_thread_test.dbc", "", "149501", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/threads/");
