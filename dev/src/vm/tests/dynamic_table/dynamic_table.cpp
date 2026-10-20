// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/exceptions.hpp>

class DynamicTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(twoDim);
		TESTER_ADD_TEST(dynArrSum);
		TESTER_ADD_TEST(lea);
		TESTER_ADD_TEST(stringOutput);
		TESTER_ADD_TEST(reallocZero);
		TESTER_ADD_TEST(reallocShrink);
		TESTER_ADD_TEST(fstToDyn);
	}

private:
	void dynArrSum() { runTestOnVm("dyn_arr_sum.dbc", "", "55", {}); }

	void lea() { runTestOnVm("lea.dbc", "", "4", {}); }

	void twoDim() { runTestOnVm("two_dim.dbc", "", "1235", {}); }

	void stringOutput() { runTestOnVm("string_output.dbc", {}, "test\ntest", { "test" }, 0); }

	void reallocZero() { runTestOnVm("realloc_zero.dbc", "", "42", {}); }

	void reallocShrink() { runTestOnVm("realloc_shrink.dbc", "", "42", {}); }

	void fstToDyn() { runTestOnVm("../common/programs/fst_to_dyn.dbc", "0", "42", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/dynamic_table/");
