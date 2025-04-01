#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/preprocessor/errors.hpp>
#include <vm/preprocessor/parser/errors.hpp>
#include <vm/preprocessor/parser/parser.hpp>

class BCParsingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(noSemicolon);
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
		fs::FilePath file(path(dbc_filename));
		auto         parsing_result = vm::parser::parse({ file });
		match_optional(parsing_result) {
			opt_err(logger) {
				std::stringstream ss;
				logger.dumpLog(true, ss);
				for (auto err_key: error_keywords) {
					assertTrue(
						ss.str().find(err_key) != std::string::npos,
						base::strConcat("Not found: ", err_key)
					);
				}
			}
			opt_some_move(_) { CORE_PANIC("Expected error, but correctly parsed."); }
		}
	}

	void invalidOpcode() {
		parseInvalidDbc(
			"invalid_opcode.dbc",
			{
				base::strConcat(vm::parser::UnknownOpCodeError::ERR_MSG, "mov_l46_imm"),
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

TESTER_COMMON_MAIN("/vm/tests/preprocessor/assembly/");
