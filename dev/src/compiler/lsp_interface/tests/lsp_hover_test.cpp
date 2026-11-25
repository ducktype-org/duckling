// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "test_utils.hpp"

#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <global_state/global_logger.hpp>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include <base/str/str_utils.hpp>

#include <diagnostic/logger.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <string>
#include <variant>
#include <vector>

namespace {
	using duck_ls_test::CompilerUnderTest;

	/**
	 * @brief A program that compiles without the standard library.
	 *
	 * The tests hover at fixed line and column numbers (from zero) in it,
	 * so adding or removing a line here means updating them.
	 * The comment on line 11 holds characters of two,
	 * three and four UTF-8 bytes, so every position below it
	 * only works if offsets are counted in code points.
	 */
	constexpr std::string_view PROGRAM
		= "class Point {\n"                       // 0
		  "    x: i64 = 0;\n"                     // 1
		  "    y: i64 = 0;\n"                     // 2
		  "}\n"                                   // 3
		  "\n"                                    // 4
		  "fun add(a: i64, b: i64) -> i64 = {\n"  // 5
		  "    var sum = a + b;\n"                // 6
		  "    return sum;\n"                     // 7
		  "}\n"                                   // 8
		  "\n"                                    // 9
		  "fun main() -> i64 = {\n"               // 10
		  "    # źółć € 😀\n"                     // 11
		  "    var p = Point(1, 2);\n"            // 12
		  "    var total = add(p.x, p.y);\n"      // 13
		  "    return total;\n"                   // 14
		  "}\n";                                  // 15

	/**
	 * @brief The Markdown of a hover, empty for a null one.
	 */
	std::string hoverText(const lsp::TextDocumentHoverResult& result) {
		if (result.isNull()) return {};
		if (const auto* markup = std::get_if<lsp::MarkupContent>(&result.value().contents))
			return markup->value;
		return "<hover contents that are not MarkupContent>";
	}
}

class LspHoverTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspHoverTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(programCompilesTest);
		TESTER_ADD_TEST(identifiersTest);
		TESTER_ADD_TEST(declarationsTest);
		TESTER_ADD_TEST(membersAndCallsTest);
		TESTER_ADD_TEST(nothingToShowTest);
		TESTER_ADD_TEST(multiByteOnTheSameLineTest);
		TESTER_ADD_TEST(closedDocumentTest);
		TESTER_ADD_TEST(brokenFileTest);
		TESTER_ADD_TEST(moreExpressionKindsTest);
		TESTER_ADD_TEST(literalsPointersAndTypesTest);
		TESTER_ADD_TEST(remainingExpressionKindsTest);
		TESTER_ADD_TEST(blockExpressionsTest);
		TESTER_ADD_TEST(rangeTest);
	}

protected:
	void beforeAll() override { duck_ls_test::setUpCompilerGlobals(); }

private:
	/**
	 * @brief Writes `program` as `pkg/pkg.dk` of a fresh workspace and opens it.
	 */
	lsp::Uri openProgram(
		duck_ls_test::VfsWorkspace& workspace,
		CompilerUnderTest&          under_test,
		std::string_view            program = PROGRAM
	) {
		workspace.add("pkg/pkg.dk", program);
		under_test.compiler.addWorkspace(workspace.uriOf());

		auto uri = workspace.uriOf("pkg/pkg.dk");
		under_test.compiler.openDocument(uri, "duckling", 1, program);
		return uri;
	}

	std::string hoverAt(CompilerUnderTest& under_test, const lsp::Uri& uri, u32 line, u32 character) {
		return hoverText(
			under_test.compiler.hover(uri, lsp::Position{ .line = line, .character = character })
		);
	}

	/**
	 * @brief The hover at `line:character` contains every one of `expected`.
	 */
	void assertHoverContains(
		CompilerUnderTest&              under_test,
		const lsp::Uri&                 uri,
		u32                             line,
		u32                             character,
		const std::vector<std::string>& expected
	) {
		const auto text  = hoverAt(under_test, uri, line, character);
		const auto where = std::to_string(line) + ":" + std::to_string(character);

		assertTrue(!text.empty(), "Expected a hover at " + where + ", got none");
		for (const auto& part: expected)
			assertTrue(
				text.find(part) != std::string::npos,
				base::strConcat("The hover at ", where, " must contain `", part, "`, got:\n", text)
			);
	}

	void assertNoHover(CompilerUnderTest& under_test, const lsp::Uri& uri, u32 line, u32 character) {
		const auto text = hoverAt(under_test, uri, line, character);
		assertTrue(
			text.empty(),
			base::strConcat(
				"Expected no hover at ",
				std::to_string(line),
				":",
				std::to_string(character),
				", got:\n",
				text
			)
		);
	}

	/**
	 * @brief The hover at `line:character` highlights the UTF-16 columns from `start` to `end`
	 * (exclusive) of the same line.
	 */
	void assertHoverRange(
		CompilerUnderTest& under_test, const lsp::Uri& uri, u32 line, u32 character, u32 start, u32 end
	) {
		const auto result
			= under_test.compiler.hover(uri, lsp::Position{ .line = line, .character = character });
		const auto where = std::to_string(line) + ":" + std::to_string(character);

		assertTrue(!result.isNull(), "Expected a hover at " + where + ", got none");
		assertTrue(result.value().range.has_value(), "Expected a range for the hover at " + where);

		const auto& range = result.value().range.value();
		assertTrue(
			range.start.line == line && range.end.line == line && range.start.character == start
				&& range.end.character == end,
			base::strConcat(
				"The hover at ",
				where,
				" must highlight columns ",
				std::to_string(start),
				" to ",
				std::to_string(end),
				", got ",
				std::to_string(range.start.line),
				":",
				std::to_string(range.start.character),
				" to ",
				std::to_string(range.end.line),
				":",
				std::to_string(range.end.character)
			)
		);
	}

	/**
	 * @brief The program the other tests use compiles, so a failure elsewhere is about
	 * hover and not about the program.
	 */
	void programCompilesTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_compiles_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		under_test.compiler.publishDiagnostics(uri);
		assertTrue(under_test.session.noErrors(uri), "PROGRAM must compile without errors");
	}

	void identifiersTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_identifiers_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverContains(under_test, uri, 7, 12, { "sum: i64" });     // return s|um;
		assertHoverContains(under_test, uri, 6, 14, { "a: i64" });       // = a| + b
		assertHoverContains(under_test, uri, 14, 12, { "total: i64" });  // below the emoji line
	}

	void declarationsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_declarations_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverContains(under_test, uri, 6, 9, { "sum: i64" });     // var s|um
		assertHoverContains(under_test, uri, 13, 9, { "total: i64" });  // var t|otal
		assertHoverContains(under_test, uri, 5, 5, { "add" });          // fun ad|d
	}

	void membersAndCallsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_members_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverContains(under_test, uri, 13, 17, { "add" });       // a|dd(
		assertHoverContains(under_test, uri, 13, 20, { "p: Point" });  // (p|.x
		assertHoverContains(under_test, uri, 13, 22, { "x: i64" });    // p.x|
		assertHoverContains(under_test, uri, 12, 13, { "Point" });     // P|oint(1, 2)
		assertHoverContains(under_test, uri, 6, 16, { "i64" });        // a |+ b: just the type
	}

	void nothingToShowTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_nothing_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertNoHover(under_test, uri, 6, 2);   // indentation
		assertNoHover(under_test, uri, 11, 8);  // inside the comment
		assertNoHover(under_test, uri, 40, 0);  // a line past the end
		assertNoHover(under_test, workspace.uriOf("pkg/missing.dk"), 0, 0);
	}

	/**
	 * @brief Characters before the identifier on its own line, including one of two UTF-16 units.
	 */
	void multiByteOnTheSameLineTest() {
		constexpr std::string_view SAME_LINE_PROGRAM
			= "fun main() -> i64 = {\n"
			  "    var x: i64 = 1;\n"
			  "    var s = \"ź😀\"; var y = x;\n"  // the x is at UTF-16 column 27
			  "    return y;\n"
			  "}\n";

		duck_ls_test::VfsWorkspace workspace("lsp_hover_same_line_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, SAME_LINE_PROGRAM);

		assertHoverContains(under_test, uri, 2, 27, { "x: i64" });
	}

	/**
	 * @brief @brief A closed document gets no hover.
	 */
	void closedDocumentTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_closed_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertTrue(under_test.compiler.closeDocument(uri).isOk(), "The document must close");
		assertNoHover(under_test, uri, 7, 12);
	}

	/**
	 * @brief A file that does not parse gets no hover, and the server keeps working.
	 */
	void brokenFileTest() {
		constexpr std::string_view BROKEN = "fun main() -> i64 = {\n    var x = ;\n}\n";

		duck_ls_test::VfsWorkspace workspace("lsp_hover_broken_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, BROKEN);

		assertNoHover(under_test, uri, 1, 8);
	}

	/**
	 * @brief Reaches into the kinds of expression `PROGRAM` does not use: match, `if` expressions,
	 * tuples, references, casts, unary minus, comparison chains, indexing and variants.
	 */
	void moreExpressionKindsTest() {
		constexpr std::string_view EXPRESSIONS_PROGRAM
			= "fun pick(v: i64 | bool) -> i64 = match (v) {\n"            // 0
			  "    case i : i64 = i;\n"                                   // 1
			  "    case b : bool = if b then 1i64 else 2i64;\n"           // 2
			  "};\n"                                                      // 3
			  "\n"                                                        // 4
			  "fun main() -> i64 = {\n"                                   // 5
			  "    var n = 3i64;\n"                                       // 6
			  "    var t = (n, 4i64);\n"                                  // 7
			  "    var r = &n;\n"                                         // 8
			  "    var c = n as i32;\n"                                   // 9
			  "    var neg = -n;\n"                                       // 10
			  "    var ok = 1i64 < n < 5i64;\n"                           // 11
			  "    var arr: i64[2];\n"                                    // 12
			  "    arr[0] = n;\n"                                         // 13
			  "    var first = arr[0] + t._1;\n"                          // 14
			  "    if (ok) return pick(n) + first + neg + (c as i64);\n"  // 15
			  "	   return 0i64;\n"                                        // 16
			  "}\n";                                                      // 17

		duck_ls_test::VfsWorkspace workspace("lsp_hover_expressions_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, EXPRESSIONS_PROGRAM);

		assertHoverContains(under_test, uri, 1, 19, { "i: i64" });       // match case: = i|;
		assertHoverContains(under_test, uri, 2, 23, { "b: bool" });      // if b| then …
		assertHoverContains(under_test, uri, 7, 13, { "n: i64" });       // (n|, 4i64)
		assertHoverContains(under_test, uri, 8, 13, { "n: i64" });       // &n|
		assertHoverContains(under_test, uri, 9, 12, { "n: i64" });       // n| as i32
		assertHoverContains(under_test, uri, 10, 15, { "n: i64" });      // -n|
		assertHoverContains(under_test, uri, 11, 20, { "n: i64" });      // 1 < n| < 5
		assertHoverContains(under_test, uri, 13, 4, { "arr: i64[2]" });  // a|rr[0] = …
		assertHoverContains(under_test, uri, 14, 25, { "t: " });         // t|._1, a tuple
		assertHoverContains(
			under_test, uri, 15, 24, { "n: i64" }
		);  // pick(n|), n becomes a variant
	}

	/**
	 * @brief Reaches into literals, pointers and type expressions, which the other programs do
	 * not use.
	 */
	void literalsPointersAndTypesTest() {
		constexpr std::string_view LITERALS_PROGRAM
			= "fun main() -> i64 = {\n"        // 0
			  "    var n = 3i64;\n"            // 1
			  "    var flag = true;\n"         // 2
			  "    var text = \"duck\";\n"     // 3
			  "    var w: i64 | bool = n;\n"   // 4
			  "    var p = &n as ptr i64;\n"   // 5
			  "    var v = *p;\n"              // 6
			  "    if (flag) return n + v;\n"  // 7
			  "    return 0i64;\n"             // 8
			  "}\n";                           // 9

		duck_ls_test::VfsWorkspace workspace("lsp_hover_literals_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, LITERALS_PROGRAM);

		assertHoverContains(under_test, uri, 2, 15, { "bool" });        // t|rue
		assertHoverContains(under_test, uri, 3, 16, { "str" });         // "d|uck"
		assertHoverContains(under_test, uri, 4, 17, { "bool: type" });  // the b|ool of i64 | bool
		assertHoverContains(under_test, uri, 5, 13, { "n: i64" });      // &n| as ptr i64
		assertHoverContains(under_test, uri, 6, 13, { "p: ptr i64" });  // *p|
	}

	/**
	 * @brief Reaches into the remaining kinds of expression: char and unit literals, array
	 * literals, `ptrof`, function pointer and optional types, lifting to an optional and `move`.
	 */
	void remainingExpressionKindsTest() {
		constexpr std::string_view REMAINING_PROGRAM
			= "fun id(x: i64) -> i64 = x;\n"           // 0
			  "\n"                                     // 1
			  "fun main() -> i64 = {\n"                // 2
			  "    var n = 3i64;\n"                    // 3
			  "    var ch: char = 'a';\n"              // 4
			  "    var u: () = ();\n"                  // 5
			  "    var arr: i64[2] = [n, 4i64];\n"     // 6
			  "    var q = ptrof n;\n"                 // 7
			  "    var f: funptr(i64) -> i64 = id;\n"  // 8
			  "    var o: ?i64 = n;\n"                 // 9
			  "    var m = move n;\n"                  // 10
			  "    return f(arr[0]);\n"                // 11
			  "}\n";                                   // 12

		duck_ls_test::VfsWorkspace workspace("lsp_hover_remaining_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, REMAINING_PROGRAM);

		assertHoverContains(under_test, uri, 4, 20, { "char" });    // '|a'
		assertHoverContains(under_test, uri, 5, 16, { "()" });      // (|)
		assertHoverContains(under_test, uri, 6, 23, { "n: i64" });  // [n|, 4i64]
		assertHoverContains(under_test, uri, 7, 12, { "ptr" });     // p|trof n: the pointer itself
		assertHoverContains(under_test, uri, 7, 18, { "n: i64" });  // ptrof n|
		assertHoverContains(
			under_test, uri, 8, 18, { "i64: type" }
		);                                                        // the i|64 of funptr(i64) -> i64
		assertHoverContains(under_test, uri, 9, 11, { "type" });  // ?|i64, the optional type
		assertHoverContains(under_test, uri, 9, 18, { "n: i64" });   // n|, lifted to ?i64
		assertHoverContains(under_test, uri, 10, 13, { "i64" });     // m|ove n: the move itself
		assertHoverContains(under_test, uri, 10, 17, { "n: i64" });  // move n|
	}

	/**
	 * @brief Reaches into block expressions: statements inside a block, its tail and a block
	 * inside a block.
	 */
	void blockExpressionsTest() {
		constexpr std::string_view BLOCKS_PROGRAM
			= "fun main() -> i64 = {\n"                                            // 0
			  "    var n = 3i64;\n"                                                // 1
			  "    var b = {\n"                                                    // 2
			  "        var y = n + 2i64;\n"                                        // 3
			  "        y * 2i64\n"                                                 // 4
			  "    };\n"                                                           // 5
			  "    var nested = { var w = { var q = n; q + 1i64 }; w * 2i64 };\n"  // 6
			  "    return b + nested;\n"                                           // 7
			  "}\n";                                                               // 8

		duck_ls_test::VfsWorkspace workspace("lsp_hover_blocks_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test, BLOCKS_PROGRAM);

		assertHoverContains(under_test, uri, 3, 16, { "n: i64" });  // var y = n| + 2i64;
		assertHoverContains(under_test, uri, 3, 12, { "y: i64" });  // var y|, declared in a block
		assertHoverContains(under_test, uri, 4, 8, { "y: i64" });   // y| * 2i64, the tail
		assertHoverContains(under_test, uri, 6, 37, { "n: i64" });  // var q = n|; in a nested block
		assertHoverContains(under_test, uri, 6, 40, { "q: i64" });  // q| + 1i64, the nested tail
	}

	/**
	 * @brief The hover tells the editor which code it describes, counted in UTF-16 units.
	 */
	void rangeTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_range_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverRange(under_test, uri, 6, 16, 14, 19);   // a |+ b: the whole `a + b`
		assertHoverRange(under_test, uri, 7, 12, 11, 14);   // return s|um;: just `sum`
		assertHoverRange(under_test, uri, 13, 19, 16, 29);  // add|(p.x, p.y): the whole call
		assertHoverRange(under_test, uri, 13, 20, 20, 21);  // (p|.x: just `p`
		assertHoverRange(under_test, uri, 13, 22, 20, 23);  // p.x|: the field access `p.x`
		assertHoverRange(under_test, uri, 12, 18, 18, 19);  // Point(1|, 2): the literal
		assertHoverRange(under_test, uri, 14, 12, 11, 16);  // return t|otal;, below the emoji line

		// A declared name has no range: the editor highlights the word under the cursor itself.
		const auto declaration = under_test.compiler.hover(
			uri, lsp::Position{ .line = 6, .character = 9 }
		);  // var s|um
		assertTrue(
			!declaration.isNull() && !declaration.value().range.has_value(),
			"The hover on a declared name must have no range"
		);

		constexpr std::string_view SAME_LINE_PROGRAM
			= "fun main() -> i64 = {\n"
			  "    var x: i64 = 1;\n"
			  "    var s = \"ź😀\"; var y = x;\n"  // the x is at UTF-16 column 27
			  "    return y;\n"
			  "}\n";

		duck_ls_test::VfsWorkspace same_line_workspace("lsp_hover_range_same_line_ws");
		CompilerUnderTest          same_line{ duck_ls_test::VfsWorkspace::vfs() };
		auto same_line_uri = openProgram(same_line_workspace, same_line, SAME_LINE_PROGRAM);

		assertHoverRange(same_line, same_line_uri, 2, 27, 27, 28);  // x after the emoji
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
