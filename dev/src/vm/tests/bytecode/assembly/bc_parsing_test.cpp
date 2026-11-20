// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

/**
 * @brief What belongs here: the text assembly parser ONLY. A test feeds it a `.dbc` file with a
 * syntax error and checks the parse error, or checks what the parser produced (e.g. source
 * positions).
 *
 * A file that parses but is semantically wrong (unknown name, bad types, bad jumps) is the
 * loader's business and goes to `loader/verification/`.
 */
class BCParsingTests: public VmParserTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCParsingTests

public:
	VM_PARSER_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(invalidOpcode);
		TESTER_ADD_TEST(noSemicolon);
		TESTER_ADD_TEST(invalidTypeSpecifier);
		TESTER_ADD_TEST(noFunctionSignature);
		TESTER_ADD_TEST(invalidLocalName);
		TESTER_ADD_TEST(cpointerBadPointee);
		TESTER_ADD_TEST(opcodeSourcePositions);
	}

private:
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

	// A literal with a suffix that is not a type ends the instruction too early.
	void invalidTypeSpecifier() {
		parseInvalidDbc(
			"invalid_type_specifier.dbc",
			{
				"Expected `;` after here",
			}
		);
	}

	// A function declared without its signature.
	void noFunctionSignature() {
		parseInvalidDbc(
			"no_func_type.dbc",
			{
				"Expected comma or `}` after here",
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
		const auto parsed_files = parseValidDbc("source_positions.dbc");

		const auto& opcodes = parsed_files.front().functions.front()->code->opcodes;

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

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/assembly/");
