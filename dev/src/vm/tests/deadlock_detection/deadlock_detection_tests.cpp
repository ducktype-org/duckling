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
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(detectionDisabledAllowsNormalMutexUse); }

private:
	TestResult runWithDetection(const std::string& filename) {
		auto pid = initProcess({ .enable_deadlock_detection = true });
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path(filename)) }));
		return runTestOnVmGetResult(pid);
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

TESTER_COMMON_MAIN("/src/vm/tests/deadlock_detection/");
