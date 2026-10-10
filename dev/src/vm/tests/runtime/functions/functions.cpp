// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBuiltinFunctions);
		TESTER_ADD_TEST(testSimpleFunctionCall);
		TESTER_ADD_TEST(testSimpleReturnValue);
		TESTER_ADD_TEST(testReturnLocal32);
		TESTER_ADD_TEST(testDifferentSizedParams);
		TESTER_ADD_TEST(testDoubleCall);
		TESTER_ADD_TEST(testDeinitializeReturnValue);
		TESTER_ADD_TEST(testRecursion);
		TESTER_ADD_TEST(testManyFunctions);
		TESTER_ADD_TEST(testPreservedFlag);
		TESTER_ADD_TEST(testGraphJumps);
		TESTER_ADD_TEST(testNoRet);
		TESTER_ADD_TEST(testSignaturesValidation);
		TESTER_ADD_TEST(testMainWithNoArguments);
	}

private:
	void testSimpleFunctionCall() {
		runTestOnVm("simple_function_call.dbc", "18", "18", {}, 42);
		runTestOnVm("simple_function_call.dbc", "1234", "1234", {}, 42);
	}

	void testSimpleReturnValue() {
		runTestOnVm("simple_return_value.dbc", "18", "18", {});
		runTestOnVm("simple_return_value.dbc", "1234", "1234", {});
	}

	void testReturnLocal32() {
		runTestOnVm("return_local32.dbc", "18", "18", {});
		runTestOnVm("return_local32.dbc", "1234", "1234", {});
	}

	void testDifferentSizedParams() {
		runTestOnVm("different_sized_params.dbc", "5 1 5", "25", {});
		runTestOnVm("different_sized_params.dbc", "42 1 31", "1302", {});
	}

	void testDoubleCall() {
		runTestOnVm("double_call.dbc", "1 2 3 4", "10", {});
		runTestOnVm("double_call.dbc", "123 456 789 100", "1468", {});
	}

	void testMainWithNoArguments() { runTestOnVm("main_no_args.dbc", "", "42", {}); }

	void testDeinitializeReturnValue() {
		loadInvalidDbc(
			"deinit_ret_val.dbc",
			{
				vm::code::RetValDeinitError::ERR_MSG,
			}
		);
	}

	void testRecursion() {
		runTestOnVm("rec_func_sum.dbc", "20", "210", {});
		runTestOnVm("rec_func_sum.dbc", "100", "5050", {});
	}

	void testManyFunctions() {
		runTestOnVm("many_functions.dbc", "5", "1115", {});
		runTestOnVm("many_functions.dbc", "21", "1131", {});
	}

	void testPreservedFlag() { runTestOnVm("preserved_flag.dbc", "", "1", {}); }

	void testBuiltinFunctions() {
		runTestOnVm("builtin_functions.dbc", "5 5", "10\n3\n", {}, 0);
		runTestOnVm("builtin_functions.dbc", "501 501", "1002\n5\n", {}, 0);
		loadInvalidDbc(
			"invalid_builtin_function.dbc",
			{
				vm::code::InvalidBuiltinFunctionError::ERR_MSG,
			}
		);
	}

	void testGraphJumps() { runTestOnVm("graph_jumps.dbc", "", "42", {}); }

	void testNoRet() {
		for (auto filename: { "no_ret.dbc", "empty_function.dbc" })
			loadInvalidDbc(
				filename,
				{
					vm::code::PathWithoutEndError::ERR_MSG,
				}
			);

		for (auto filename: { "infinite_loop.dbc", "dead_end.dbc" }) loadValidDbc(filename);
	}

	void testSignaturesValidation() {
		loadInvalidDbc(
			"invalid_ret_type.dbc",
			{
				vm::code::UnknownTypeError::ERR_MSG,
			}
		);
		loadInvalidDbc(
			"invalid_param_type.dbc",
			{
				vm::code::UnknownTypeError::ERR_MSG,
			}
		);
		loadInvalidDbc(
			"casting_invalid_ret.dbc",
			{
				vm::code::InvalidRetError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/functions/");
