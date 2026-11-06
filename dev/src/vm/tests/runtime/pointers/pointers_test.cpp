// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <string>

/**
 * @brief What belongs here: pointers while a program runs. That covers pointers to locals,
 * globals and heap blocks, copying them, the blocks a local gets when its address is taken, and
 * data structures built from pointers (linked lists).
 *
 * A pointer the VM must refuse at run time (null, freed, out of bounds) goes to
 * `runtime/errors/`.
 */
class VmPointersTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmPointersTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(pointerTest);
		TESTER_ADD_TEST(referenceOnColdBranchTest);
		TESTER_ADD_TEST(localSlotAddressingTest);
		TESTER_ADD_TEST(localSlotAddressingInCalleeTest);
		TESTER_ADD_TEST(linkedListTest);
		TESTER_ADD_TEST(linkedListNoStructLoadStoreTest);
	}

private:
	void pointerTest() {
		for (auto filename:
		     { "pointer_to_local.dbc", "pointer_copy.dbc", "pointer_to_passed_blocks.dbc" }) {
			runTestOnVm(filename, "", "42");
		}
		runTestOnVm("pointer_to_global.dbc", {}, "429913371337", {}, 1'337);
	}

	/**
	 * @brief A variable whose address is only taken on a path that is not executed never gets a
	 * block, and the process must still validate its memory cleanly.
	 */
	void referenceOnColdBranchTest() { runTestOnVm("reference_on_cold_branch.dbc", "", "42"); }

	void localSlotAddressingTest() { runTestOnVm("local_slot_addressing.dbc", "", "7724"); }

	void localSlotAddressingInCalleeTest() {
		runTestOnVm("local_slot_addressing_in_callee.dbc", "", "42422");
	}

	void linkedListTest() { runTestOnVm("linked_list.dbc", "3 11 22 33", "332211"); }

	void linkedListNoStructLoadStoreTest() {
		runTestOnVm("linked_list_no_struct_load_store.dbc", "3 11 22 33", "332211");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/pointers/");
