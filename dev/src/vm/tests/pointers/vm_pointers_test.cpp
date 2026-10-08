// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <string>

class VmPointersTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmPointersTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(linkedListTest);
		TESTER_ADD_TEST(linkedListNoStructLoadStoreTest);
		TESTER_ADD_TEST(ptrPartsTest);
	}

private:
	void linkedListTest() { runTestOnVm("linked_list.dbc", "3 11 22 33", "332211"); }

	void linkedListNoStructLoadStoreTest() {
		runTestOnVm("linked_list_no_struct_load_store.dbc", "3 11 22 33", "332211");
	}

	// The offsets of the struct and of its second field, the two ids being equal, and then the
	// null pointer failing to decompose the way a dereference does.
	void ptrPartsTest() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("ptr_parts.dbc", "", "080"), "Accessing null pointer"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/pointers/");
