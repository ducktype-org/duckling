// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/str/str_utils.hpp>

#include <vm/bytecode/validator/errors.hpp>

class BCBuildingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCBuildingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(labelNotFound);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(unknownFunction);
		TESTER_ADD_TEST(invalidFunctionCall);
		TESTER_ADD_TEST(invalidRetTypeInCall);
	}

private:
	void multipleLabels() {
		loadInvalidDbc(
			"multiple_labels.dbc",
			{
				vm::code::DuplicatedLabelError::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		loadInvalidDbc(
			"label_not_found.dbc",
			{
				vm::code::UnknownLabelError::ERR_MSG,
				"LB",
			}
		);
	}

	void unknownType() {
		loadInvalidDbc(
			"unknown_type.dbc",
			{
				base::strConcat(vm::code::UnknownTypeError::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		loadInvalidDbc(
			"unknown_function.dbc",
			{
				base::strConcat(vm::code::UnknownFunctionError::ERR_MSG, "foo"),
			}
		);
	}

	void invalidFunctionCall() {
		loadInvalidDbc(
			"invalid_function_call.dbc",
			{
				vm::code::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}

	void invalidRetTypeInCall() {
		loadInvalidDbc(
			"invalid_ret_type_in_call.dbc",
			{
				vm::code::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/assembly/");
