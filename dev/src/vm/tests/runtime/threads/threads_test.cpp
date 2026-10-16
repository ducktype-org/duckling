// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

/**
 * @brief What belongs here: threads that run correctly. That covers starting and joining threads,
 * reusing a finished thread, mutexes and condition variables.
 *
 * A threading error the VM must detect (a deadlock, a locked mutex being destroyed, a panic in
 * one of several threads) goes to `runtime/errors/threads/`.
 */
class VmThreadTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadingTest);
		TESTER_ADD_TEST(mutexTest);
		TESTER_ADD_TEST(cvTest);
		TESTER_ADD_TEST(reuseThreadTest);
		TESTER_ADD_TEST(detectionDisabledAllowsNormalMutexUse);
	}

private:
	void multithreadingTest() {
		// We don't check result here - only if it finished successfully
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

	void reuseThreadTest() { runTestOnVm("reuse_thread_test.dbc", "", "149501", {}); }

	void detectionDisabledAllowsNormalMutexUse() {
		// Spawn a process with deadlock detection disabled (the default) and verify normal mutex
		// usage still works.
		auto pid  = initProcess({});
		auto file = fs::File(path("safe_mutex.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));
		handleTestResult(runTestOnVmGetResult(pid), 0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/threads/");
