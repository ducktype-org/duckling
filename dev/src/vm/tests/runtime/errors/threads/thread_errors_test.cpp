// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/safe/exceptions.hpp>

/**
 * @brief What belongs here: errors the VM detects while SEVERAL threads run. That covers a panic
 * in one thread taking the whole process down, a locked mutex being destroyed, and the deadlock
 * detector (it must report real deadlocks).
 *
 * This is a separate binary from `runtime_errors_test` because these programs run long loops in
 * worker threads, which the TC flavour can only run in an optimised build.
 */
class VmThreadErrorsTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadErrorsTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadZeroDiv);
		TESTER_ADD_TEST(destroyLockedMutexTest);
		TESTER_ADD_TEST(deadlockTest);
		TESTER_ADD_TEST(deadlockWakingFromCv);
		TESTER_ADD_TEST(deadlockWakingFromCvMain);
	}

private:
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
		// Two threads deadlock and main divides by zero. The deadlock detection is off here, so the
		// panic in main is what must take the process down with the deadlocked threads.
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("deadlock_then_panic.dbc"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
	}

	void destroyLockedMutexTest() {
		// Destroying a mutex that is still held is a program error the VM must report.
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("destroy_locked_mutex.dbc"),
			vm::exceptions::VMDestroyLockedMutexException::ERR_MSG
		);
	}

	TestResult runWithDetection(const std::string& filename) {
		auto pid = initProcess({ .enable_deadlock_detection = true });
		assertSucceeded(
			vm::api::loadFiles(pid, { fs::File(path(filename)) }),
			base::strConcat("Load of '", filename, "'")
		);
		return runTestOnVmGetResult(pid);
	}

	void deadlockTest() {
		// Two threads wait on a barrier and when released they deadlock. Main waits for them by
		// joining.
		for (int i = 0; i < 5; ++i) {
			assertExecutionPanickedWithAndKill(
				runWithDetection("deterministic_deadlock.dbc"),
				vm::exceptions::VMDeadlockException::ERR_MSG
			);
		}
	}

	void deadlockWakingFromCv() {
		// One thread sleeps on CV while main and other thread deadlock
		assertExecutionPanickedWithAndKill(
			runWithDetection("cv_deadlock.dbc"), vm::exceptions::VMDeadlockException::ERR_MSG
		);
	}

	void deadlockWakingFromCvMain() {
		// Two threads deadlock and wake main that will finish execution
		assertExecutionPanickedWithAndKill(
			runWithDetection("cv_main_deadlock.dbc"), vm::exceptions::VMDeadlockException::ERR_MSG
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/errors/threads/");
