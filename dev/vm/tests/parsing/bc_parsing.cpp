#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <variant>
#include <vm_tester_utils.hpp>
#include <vm/preprocessor/parser/errors.hpp>

class BCParsingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(repeatedTypes);
		TESTER_ADD_TEST(noSemicolon);
		TESTER_ADD_TEST(labelNotFound);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(unknownFunction);
		TESTER_ADD_TEST(invalidLiteral);
	}

private:
	void multipleLabels() {
		parseInvalidDbc(
			"multiple_labels.dbc",
			{
				base::strConcat(vm::parser::InvalidLabel::ERR_MSG, "Repeated label"),
				vm::parser::RepeatedLabelNote::ERR_MSG,
			}
		);
	}

	void invalidOpcode() {
		parseInvalidDbc(
			"invalid_opcode.dbc",
			{
				base::strConcat(vm::parser::UnknownOpCodeError::ERR_MSG, "mov_l46_imm"),
			}
		);
	}

	void repeatedTypes() {
		parseInvalidDbc(
			"repeated_types.dbc",
			{
				vm::parser::DuplicatedTypeError::ERR_MSG,
				vm::parser::DuplicatedTypeNote::ERR_MSG,
			}
		);
	}

	void noSemicolon() {
		parseInvalidDbc(
			"no_semicolon.dbc",
			{
				vm::parser::ExpectedSemicolonAfterError::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		parseInvalidDbc(
			"label_not_found.dbc",
			{
				base::strConcat(vm::parser::InvalidLabel::ERR_MSG, "Label does not exist."),
			}
		);
	}

	void unknownType() {
		parseInvalidDbc(
			"unknown_type.dbc",
			{
				base::strConcat(vm::parser::UnknownType::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		parseInvalidDbc(
			"unknown_function.dbc",
			{
				base::strConcat(vm::parser::UnknownFunction::ERR_MSG, "foo"),
			}
		);
	}

	void invalidLiteral() {
		parseInvalidDbc(
			"invalid_literal.dbc",
			{
				base::strConcat(
					vm::parser::InvalidLiteral::ERR_MSG,
					"Not a valid number for `vm::opargs::StackLocalI64`"
				),
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/parsing/");
