#include <vm_tester_utils.hpp>

#include <vm/loader/parser/errors.hpp>
#include <vm/loader/parser/parser.hpp>

using namespace vm::loader;

class BCParsingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(noSemicolon);
		TESTER_ADD_TEST(invalidLocalName);
	}

private:
	/**
	 * @brief Parses a syntactically incorrect file. Asserts that `error_keywords` are present in
	 * the error message.
	 */
	void parseInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	) {
		fs::File file(path(dbc_filename));
		auto     parsing_result = parser::parse({ file });
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
			opt_some(_) { CORE_PANIC("Expected error, but correctly parsed."); }
		}
	}

	void invalidOpcode() {
		parseInvalidDbc(
			"invalid_opcode.dbc",
			{
				base::strConcat(parser::UnknownOpCodeError::ERR_MSG, "mov_l46_imm"),
			}
		);
	}

	void noSemicolon() {
		parseInvalidDbc(
			"no_semicolon.dbc",
			{
				parser::ExpectedSemicolonAfterError::ERR_MSG,
			}
		);
	}

	void invalidLocalName() {
		parseInvalidDbc("invalid_local_name.dbc", { tpc::NoIdentifierErrorOld::ERR_MSG });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/assembly/");
