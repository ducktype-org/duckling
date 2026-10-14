// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

/**
 * @brief What belongs here: instructions on primitive values (integers of every width, unsigned
 * and boolean values, literals, casts, `void`) giving the right result when a program runs.
 *
 * A runtime error on a primitive (e.g. a zero division) goes to `runtime/errors/`.
 */
class VmPrimitivesTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmPrimitivesTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(initPrimitivesWithZero);
		TESTER_ADD_TEST(check8BitsInstructions);
		TESTER_ADD_TEST(check16BitsInstructions);
		TESTER_ADD_TEST(check32BitsInstructions);
		TESTER_ADD_TEST(check64BitsInstructions);
		TESTER_ADD_TEST(checkVoidTypeValid);
		TESTER_ADD_TEST(verySimpleUnsignedTest);
		TESTER_ADD_TEST(verySimpleBooleanTest);
		TESTER_ADD_TEST(literalsTest);
		TESTER_ADD_TEST(checkCastingInstructions);
	}

private:
	void checkCastingInstructions() { runTestOnVm("casting.dbc", "", "11111111111", {}); }

	void initPrimitivesWithZero() { runTestOnVm("init_primitives_with_zero.dbc", "", "0", {}); }

	void check8BitsInstructions() { runTestOnVm("8bits.dbc", "", "1", {}); }

	void check16BitsInstructions() { runTestOnVm("16bits.dbc", "", "1", {}); }

	void check32BitsInstructions() { runTestOnVm("32bits.dbc", "", "11", {}); }

	void check64BitsInstructions() { runTestOnVm("64bits.dbc", "", "11", {}); }

	void checkVoidTypeValid() { runTestOnVm("valid_void_type.dbc", "", "2", {}); }

	void literalsTest() {
		runTestOnVm("literals_test_32.dbc", "", "1", {});
		runTestOnVm("literals_test_64.dbc", "", "1", {});
	}

	void verySimpleUnsignedTest() { runTestOnVm("very_simple_unsigned.dbc", "", "1235", {}); }

	void verySimpleBooleanTest() { runTestOnVm("very_simple_boolean.dbc", "", "1", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/primitives/");
