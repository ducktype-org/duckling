// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		TESTER_ADD_TEST(deadlockWakingFromCvMain);
		TESTER_ADD_TEST(detectionDisabledAllowsNormalMutexUse);
	}

private:
	TestResult runWithDetection(const std::string& filename) {
		auto pid = initProcess({ .enable_deadlock_detection = true });
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path(filename)) }));
		return runTestOnVmGetResult(pid);
	}

	void deadlockTest() {
		// Two threads wait on a barrier and when released they deadlock. Main waits for them by joining.
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

	void detectionDisabledAllowsNormalMutexUse() {
		// Spawn a process with deadlock detection disabled (the default) and verify normal mutex
		// usage still works.
		auto pid  = initProcess({});
		auto file = fs::File(path("safe_mutex.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));
		handleTestResult(runTestOnVmGetResult(pid), 0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/threads/deadlock_detection/");
