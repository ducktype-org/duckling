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

		assertHoverContains(under_test, uri, 7, 12, { "sum", "i64" });     // return s|um;
		assertHoverContains(under_test, uri, 6, 14, { "a", "i64" });       // = a| + b
		assertHoverContains(under_test, uri, 14, 12, { "total", "i64" });  // below the emoji line
	}

	void declarationsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_declarations_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverContains(under_test, uri, 6, 9, { "sum", "i64" });     // var s|um
		assertHoverContains(under_test, uri, 13, 9, { "total", "i64" });  // var t|otal
		assertHoverContains(under_test, uri, 5, 5, { "add" });            // fun ad|d
	}

	void membersAndCallsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_members_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertHoverContains(under_test, uri, 13, 17, { "add" });         // a|dd(
		assertHoverContains(under_test, uri, 13, 20, { "p", "Point" });  // (p|.x
		assertHoverContains(under_test, uri, 13, 22, { "x", "i64" });    // p.x|
		assertHoverContains(under_test, uri, 12, 13, { "Point" });       // P|oint(1, 2)
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

		assertHoverContains(under_test, uri, 2, 27, { "x", "i64" });
	}

	/**
	 * @brief Without a buffer the hover reads the file on disk.
	 */
	void closedDocumentTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_hover_closed_ws");
		CompilerUnderTest          under_test{ duck_ls_test::VfsWorkspace::vfs() };
		auto                       uri = openProgram(workspace, under_test);

		assertTrue(under_test.compiler.closeDocument(uri).isOk(), "The document must close");
		assertHoverContains(under_test, uri, 7, 12, { "sum", "i64" });
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
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
