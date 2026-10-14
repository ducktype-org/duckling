// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

/**
 * @brief What belongs here: dynamic tables (arrays sized at run time) while a program runs. That
 * covers allocation, reallocation (growing, shrinking, to zero), element access, nested tables,
 * and turning a fixed-size table into a dynamic one.
 *
 * A dynamic-table operation the VM must refuse (too large, wrong type, out of bounds) goes to
 * `runtime/errors/`.
 */
class DynamicTableVmTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicTableVmTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
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

	void fstToDyn() { runTestOnVm("../../../common/programs/fst_to_dyn.dbc", "0", "42", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/arrays/dynamic_table/");
