#include <vm_tester_utils.hpp>

#include <base/str_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/parser/errors.hpp>
#include <vm/loader/parser/parser.hpp>

class BCBuildingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCBuildingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(repeatedTypes);
		TESTER_ADD_TEST(labelNotFound);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(unknownFunction);
		TESTER_ADD_TEST(noFuncType);
		TESTER_ADD_TEST(invalidFunctionCall);
		TESTER_ADD_TEST(invalidRetTypeInCall);
	}

private:
	void multipleLabels() {
		loadInvalidDbc(
			"multiple_labels.dbc",
			{
				vm::code::builders::DuplicateLabelError::ERR_MSG,
			}
		);
	}

	void repeatedTypes() {
		loadInvalidDbc(
			"repeated_types.dbc",
			{
				vm::loader::DuplicatedTypeError::ERR_MSG,
				vm::loader::DuplicatedTypeNote::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		loadInvalidDbc(
			"label_not_found.dbc",
			{
				vm::code::builders::UnknownLabelError::ERR_MSG,
				"LB",
			}
		);
	}

	void unknownType() {
		loadInvalidDbc(
			"unknown_type.dbc",
			{
				base::strConcat(vm::loader::UnknownTypeError::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		loadInvalidDbc(
			"unknown_function.dbc",
			{
				base::strConcat(vm::code::builders::UnknownFunctionError::ERR_MSG, "foo"),
			}
		);
	}

	void noFuncType() {
		loadInvalidDbc(
			"no_func_type.dbc",
			{
				base::strConcat(vm::code::builders::MissingFunctionalTypeError::ERR_MSG, "foo"),
			}
		);
	}

	void invalidFunctionCall() {
		loadInvalidDbc(
			"invalid_function_call.dbc",
			{
				vm::code::builders::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}

	void invalidRetTypeInCall() {
		loadInvalidDbc(
			"invalid_ret_type_in_call.dbc",
			{
				vm::code::builders::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/loader/assembly/");
