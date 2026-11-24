// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

/**
 * @brief What belongs here: jumps and labels inside one function, i.e. that a program takes the
 * branch it should and that locals stay correct on every path through the jump graph.
 *
 * Calls between functions go to `runtime/functions/`. A jump the loader must reject goes to
 * `loader/verification/wrong/jumps/`.
 */
class VmControlFlowTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmControlFlowTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jump);
		TESTER_ADD_TEST(testGraphJumps);
	}

private:
	void jump() { runTestOnVm("jump.dbc", "", "5", {}); }

	void testGraphJumps() { runTestOnVm("graph_jumps.dbc", "", "42", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/control_flow/");
