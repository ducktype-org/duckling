// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/loader/parser/parser.hpp>

#include <sstream>

using namespace vm::loader;

class BCParsingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(noSemicolon);
		TESTER_ADD_TEST(invalidLocalName);
		TESTER_ADD_TEST(cpointerBadPointee);
		TESTER_ADD_TEST(opcodeSourcePositions);
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
				"OpCode 'mov_l46_imm' does not exist.",
			}
		);
	}

	void noSemicolon() {
		parseInvalidDbc(
			"no_semicolon.dbc",
			{
				"Expected `;` after here",
			}
		);
	}

	void invalidLocalName() {
		parseInvalidDbc("invalid_local_name.dbc", { "Expected an identifier here" });
	}

	void cpointerBadPointee() {
		parseInvalidDbc(
			"cpointer_bad_pointee.dbc",
			{ "Expected an identifier (pointee type) or end of declaration." }
		);
	}

	/**
	 * @brief Asserts that opcode source positions span exactly the instruction text, i.e. that
	 * the (inclusive) `.end` covers the last argument and nothing past it.
	 */
	void opcodeSourcePositions() {
		fs::File file(path("source_positions.dbc"));
		auto     parsing_result = parser::parse({ file });
		ASSERT_HAS_VALUE(parsing_result, "Expected successful parse.");

		const auto& opcodes = parsing_result->front().functions.front()->code->opcodes;

		const std::vector<std::string> expected = {
			"init_pany_type x, i32",
			"mov_p32_imm x, 5",
			"mov_p32_imm x, -17",
			"jmp_label end",
			"label end",
			"output_p32 x",
			"deinit",
			"ret",
		};

		assertEqual(opcodes.size(), expected.size(), "Unexpected opcode count.");
		for (usize i = 0; i < expected.size(); i++)
			assertEqual(
				expected[i],
				opcodes[i]->position.content(),
				base::strConcat("Wrong position content for opcode #", std::to_string(i))
			);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/assembly/");
