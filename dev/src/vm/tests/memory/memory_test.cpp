// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmMemoryTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmMemoryTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noDoubleDestructorCalls);
		TESTER_ADD_TEST(variantDestructor);
	}

private:
	void noDoubleDestructorCalls() { runTestOnVm("../common/programs/no_double_destructor.dbc"); }

	void variantDestructor() { runTestOnVm("variant_destructor.dbc"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
