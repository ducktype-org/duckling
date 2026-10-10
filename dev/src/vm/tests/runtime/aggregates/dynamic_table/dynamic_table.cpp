// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/exceptions.hpp>

class DynamicTableVmTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicTableVmTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(twoDim);
		TESTER_ADD_TEST(dynArrSum);
		TESTER_ADD_TEST(lea);
		TESTER_ADD_TEST(tooLarge);
		TESTER_ADD_TEST(stringOutput);
		TESTER_ADD_TEST(reallocZero);
		TESTER_ADD_TEST(reallocShrink);
		TESTER_ADD_TEST(fstToDyn);
		TESTER_ADD_TEST(reallocOnFst);
	}

private:
	void dynArrSum() { runTestOnVm("dyn_arr_sum.dbc", "", "55", {}); }

	void lea() { runTestOnVm("lea.dbc", "", "4", {}); }

	void twoDim() { runTestOnVm("two_dim.dbc", "", "1235", {}); }

	void tooLarge() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("too_large.dbc", "", "9223372036854775808"),
			vm::exceptions::VMMemoryAllocationError::ERR_MSG
		);
	}

	void stringOutput() { runTestOnVm("string_output.dbc", {}, "test\ntest", { "test" }, 0); }

	void reallocZero() { runTestOnVm("realloc_zero.dbc", "", "42", {}); }

	void reallocShrink() { runTestOnVm("realloc_shrink.dbc", "", "42", {}); }

	void fstToDyn() {
		runTestOnVm("fst_to_dyn.dbc", "0", "42", {});
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("fst_to_dyn.dbc", "1"),
			vm::exceptions::VMOutOfBlockBoundsException::ERR_MSG
		);
	}

	void reallocOnFst() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("realloc_on_fst.dbc", ""),
			vm::exceptions::VMDynTableReAllocTypeMismatch::ERR_MSG
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/aggregates/dynamic_table/");
