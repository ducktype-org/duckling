#include <api/data/core_operation_error.hpp>
#include <api/data/load_program_error.hpp>
#include <variant>
#include <vm_tester_utils.hpp>
#include <preprocessor/parser/errors.hpp>

class BCParsingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(noMain);
		TESTER_ADD_TEST(repeatedTypes);
		TESTER_ADD_TEST(noSemicolon);
		TESTER_ADD_TEST(labelNotFound);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(unknownFunction);
		TESTER_ADD_TEST(invalidLiteral);
	}

private:
	/**
	 * @brief Parses a syntactically incorrect file. Asserts that `error_keywords` are present in
	 * the error message.
	 */
	void parseInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	) {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response.expect("Spawn failed").pid;

		fs::FilePath file(path(dbc_filename));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		ASSERT_TRUE(loaded_file_response.has_error());
		auto err = loaded_file_response.error();
		ASSERT_TRUE(std::holds_alternative<vm::api::CoreOperationError>(err));
		auto core_op = std::get<vm::api::CoreOperationError>(err);
		ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(core_op));
		auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
		// std::cerr << err_str << '\n';
		for (auto err_key: error_keywords) {
			assertTrue(
				err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
			);
		}
	}

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

	void noMain() {
		parseInvalidDbc(
			"no_main.dbc",
			{
				vm::parser::NoMainError::ERR_MSG,
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
					"Not a valid number for `vm::opargs::StackOffset`"
				),
			}
		);
	}
};

TESTER_COMMON_MAIN("/VM/tests/parsing/");
