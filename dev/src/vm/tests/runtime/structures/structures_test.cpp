// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

/**
 * @brief What belongs here: structures (`data` types) while a program runs. That covers reading
 * and writing fields, and structures nested inside other aggregates.
 *
 * Arrays go to `runtime/arrays/`, variants to `runtime/variants/`.
 */
class VmStructuresTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmStructuresTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(structureOperations);
		TESTER_ADD_TEST(nestedAggregateTypesCorrectness);
	}

private:
	void structureOperations() { runTestOnVm("structure_operations.dbc", "", "506", {}); }

	void nestedAggregateTypesCorrectness() {
		runTestOnVm("nested_aggregate.dbc", "", "133707770999", {});
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/structures/");
