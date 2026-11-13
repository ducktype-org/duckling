// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <lsp_interface/positions.hpp>

#include <tester/tester.hpp>

#include <string_view>

class LspPositionsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspPositionsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(codePointOffsetTest); }

private:
	/**
	 * @brief Checks that `toCodePointOffset` turns client positions into code point offsets.
	 *
	 * Covers all three line breaks, multi-byte characters, a character that takes two UTF-16
	 * code units, and positions past the end of a line or of the text.
	 */
	void codePointOffsetTest() {
		auto at = [](std::string_view text, u32 line, u32 character) {
			return duck_ls::toCodePointOffset(
				text, lsp::Position{ .line = line, .character = character }
			);
		};

		assertTrue(at("abc", 0, 0).value() == 0, "Start of the text");
		assertTrue(at("abc\ndef", 1, 1).value() == 5, "Second line");
		assertTrue(at("abc\r\ndef", 1, 0).value() == 5, "CRLF is two code points");
		assertTrue(at("abc\rdef", 1, 2).value() == 6, "A lone CR is a line break");
		assertTrue(at("źó\nx", 1, 0).value() == 3, "Two-byte characters are one code point");
		assertTrue(at("a🦆b", 0, 3).value() == 2, "An emoji is two UTF-16 units");
		assertTrue(at("a🦆b", 0, 2).value() == 1, "A column inside a pair stays before it");
		assertTrue(at("abc\n", 0, 3).value() == 3, "The column right at the line end");
		assertTrue(at("abc\n", 0, 99).value() == 3, "A long column clamps to the line end");
		assertTrue(at("abc", 2, 0).empty(), "A missing line has no offset");
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
