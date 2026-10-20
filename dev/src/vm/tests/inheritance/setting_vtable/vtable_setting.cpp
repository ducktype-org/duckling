// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/validator/errors.hpp>

class VmSettingVtableCorrectness: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmSettingVtableCorrectness

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(settingVtableCorrectness); }

private:
	void settingVtableCorrectness() {
		runTestOnVm("setting_vtable/table_of_virtual.dbc", {}, { "0\n" }, {}, 0);
		runTestOnVm("setting_vtable/changing_vtable.dbc", {}, { "1\n0\n" }, {}, 0);
		runTestOnVm("setting_vtable/variant_of_virtual.dbc", {}, { "0\n" }, {}, 0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/inheritance/");
