// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/collections/optional.hpp>

/**
 * @brief What belongs here: classes and interfaces while a program runs. That covers virtual
 * calls being dispatched to the right method, upcasts and downcasts, and setting the vtable of an
 * object (`setting_vtable/`), including objects stored in tables and variants, and changing a
 * vtable that was already set.
 *
 * An inheritance error the loader must reject goes to `loader/inheritance/`.
 */
class VmInheritanceSemanticsTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceSemanticsTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(dynamicDispatch);
		TESTER_ADD_TEST(upcast);
		TESTER_ADD_TEST(downcast);
		TESTER_ADD_TEST(settingVtableCorrectness);
	}

private:
	void downcast() { runTestOnVm("semantics/downcast.dbc", "", "10", {}, 0); }

	void upcast() { runTestOnVm("semantics/valid_upcast.dbc", {}, {}, {}, 0); }

	void dynamicDispatch() {
		runTestOnVm("semantics/simple_dispatch.dbc", "", "421", {}, 0);
		runTestOnVm("semantics/interface_dispatch.dbc", "", "11224455", {}, 0);
		runTestOnVm("semantics/dynamic_dispatch.dbc", "", "44542321", {}, 0);
		runTestOnVm("semantics/method_call_with_args.dbc", "12 13", "25", {}, 0);
	}

	void settingVtableCorrectness() {
		runTestOnVm("setting_vtable/table_of_virtual.dbc", {}, { "0\n" }, {}, 0);
		runTestOnVm("setting_vtable/changing_vtable.dbc", {}, { "1\n0\n" }, {}, 0);
		runTestOnVm("setting_vtable/variant_of_virtual.dbc", {}, { "0\n" }, {}, 0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/inheritance/");
