// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

/**
 * @brief What belongs here: global variables while a program runs. That covers their initial
 * values, their constructors and destructors, and reading and writing them from code.
 *
 * A global the loader must reject goes to `loader/verification/wrong/globals/`. A global that
 * leaks goes to `runtime/errors/`.
 */
class VmGlobalsTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmGlobalsTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(globalInitializationTest);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(globalDestructorTest);
		TESTER_ADD_TEST(globalInitialValueTest);
	}

private:
	void globalsTest() { runTestOnVm("globals.dbc", {}, "5", {}, 5); }

	void globalInitializationTest() { runTestOnVm("global_initialization.dbc", {}, {}, {}, 5); }

	void globalsInitializationTest() { runTestOnVm("globals_initialization.dbc", {}, {}, {}, 7); }

	void globalDestructorTest() { runTestOnVm("global_destructor.dbc", {}, {}, {}, 5); }

	void globalInitialValueTest() {
		runTestOnVm("global_initial_value.dbc", "", "10\n-10\n/1\n-1\no", {});
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/globals/");
