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
#include <lsp_interface/semantic_tokens.hpp>
#include <lsp_interface/server_session.hpp>

#include <diagnostic/logger.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace {
	using TokenType = lsp::SemanticTokenTypes;

	/**
	 * @brief A compiler over the singleton VFS, wired to a session that collects what it is
	 * asked to push.
	 */
	struct CompilerUnderTest final {
		CompilerUnderTest():
			  files(duck_ls_test::VfsWorkspace::vfs()),
			  compiler{ &session, &files } {}

		duck_ls_test::StringStream      stream{ "" };
		lsp::ServerEndpoint             endpoint{ stream };
		duck_ls_test::CollectingSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache             files;
		duck_ls::Compiler               compiler;
	};

	/**
	 * @brief One token with its position made absolute again, and the text it covers.
	 */
	struct DecodedToken final {
		lsp::Uint   line      = 0;
		lsp::Uint   character = 0;
		lsp::Uint   length    = 0;
		TokenType   type      = TokenType::MAX_VALUE;
		std::string text{};
	};

	std::vector<std::string> splitLines(std::string_view source) {
		std::vector<std::string> lines{ "" };
		for (char c: source)
			if (c == '\n')
				lines.emplace_back();
			else
				lines.back() += c;
		return lines;
	}

	/**
	 * @brief Undoes the relative encoding of `tokens`, reading the covered text from `source`.
	 */
	std::vector<DecodedToken> decode(const lsp::SemanticTokens& tokens, std::string_view source) {
		auto lines = splitLines(source);

		std::vector<DecodedToken> result;
		lsp::Uint                 line      = 0;
		lsp::Uint                 character = 0;
		for (usize i = 0; i + 5 <= tokens.data.size(); i += 5) {
			const auto delta_line = tokens.data[i];
			line += delta_line;
			character = delta_line == 0 ? character + tokens.data[i + 1] : tokens.data[i + 1];

			DecodedToken token{
				.line      = line,
				.character = character,
				.length    = tokens.data[i + 2],
				.type      = static_cast<TokenType>(tokens.data[i + 3]),
			};
			if (line < lines.size()) token.text = lines[line].substr(character, token.length);
			result.push_back(std::move(token));
		}
		return result;
	}
}

class LspSemanticTokensTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspSemanticTokensTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(legendTest);
		TESTER_ADD_TEST(lexicalKindsTest);
		TESTER_ADD_TEST(declarationsAndUsesTest);
		TESTER_ADD_TEST(classMembersTest);
		TESTER_ADD_TEST(constTypeTest);
		TESTER_ADD_TEST(importedModuleTest);
		TESTER_ADD_TEST(encodingTest);
		TESTER_ADD_TEST(brokenPackageIsLexicalOnlyTest);
		TESTER_ADD_TEST(followsEditsTest);
		TESTER_ADD_TEST(unknownDocumentTest);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		query::setTrackReverseGraph(true);
		compiler::frontend::use_module_modifier_remove = true;
	}

private:
	CompilerUnderTest under_test;

	/**
	 * @brief Opens `content` as the main file of a fresh package and returns its decoded tokens.
	 */
	std::vector<DecodedToken> tokensOf(std::string_view workspace_name, std::string_view content) {
		duck_ls_test::VfsWorkspace workspace(workspace_name);
		workspace.add("pkg/pkg.dk", content);
		const auto main_uri = workspace.uriOf("pkg/pkg.dk");

		under_test.compiler.addWorkspace(workspace.uriOf());
		under_test.compiler.openDocument(main_uri, "duckling", 1, content);

		auto tokens = under_test.compiler.semanticTokens(main_uri);
		assertTrue(tokens.has_value(), "An opened document must have semantic tokens");
		return decode(tokens.value(), content);
	}

	/**
	 * @brief Asserts that the `occurrence`-th token covering exactly `text` has the type
	 * `expected`.
	 */
	void assertTokenType(
		const std::vector<DecodedToken>& tokens,
		std::string_view                 text,
		TokenType                        expected,
		usize                            occurrence = 0
	) {
		std::string what = std::string(text) + "#" + std::to_string(occurrence);
		for (const auto& token: tokens) {
			if (token.text != text) continue;
			if (occurrence-- > 0) continue;
			assertTrue(
				token.type == expected,
				what + " must be " + std::string(lsp::SemanticTokenTypesEnum(expected).value())
					+ ", was " + std::string(lsp::SemanticTokenTypesEnum(token.type).value())
			);
			return;
		}
		std::string rendered;
		for (const auto& token: tokens)
			rendered += "[" + token.text + ":"
			          + std::string(lsp::SemanticTokenTypesEnum(token.type).value()) + "] ";
		assertTrue(false, "No token " + what + " in " + rendered);
	}

	void legendTest() {
		auto legend = duck_ls::semanticTokensLegend();

		assertTrue(legend.tokenModifiers.empty(), "No modifiers are reported");
		assertTrue(
			legend.tokenTypes.size() == static_cast<usize>(TokenType::MAX_VALUE),
			"Every standard token type must be in the legend"
		);
		assertTrue(
			legend.tokenTypes[static_cast<usize>(TokenType::Namespace)] == "namespace",
			"The legend must start with namespace"
		);
		assertTrue(
			legend.tokenTypes[static_cast<usize>(TokenType::Function)] == "function",
			"An index must name the type it encodes"
		);
	}

	void lexicalKindsTest() {
		auto tokens = tokensOf(
			"lsp_sem_lexical_ws",
			"fun main() -> i64 = {\n"
			"    var s = \"abc\";\n"
			"    return 40 + 2;\n"
			"}\n"
		);

		assertTokenType(tokens, "fun", TokenType::Keyword);
		assertTokenType(tokens, "return", TokenType::Keyword);
		assertTokenType(tokens, "\"abc\"", TokenType::String);
		assertTokenType(tokens, "40", TokenType::Number);
		assertTokenType(tokens, "2", TokenType::Number);
		assertTokenType(tokens, "+", TokenType::Operator);
	}

	void declarationsAndUsesTest() {
		auto tokens = tokensOf(
			"lsp_sem_decl_ws",
			"fun add(a: i64, b: i64) -> i64 = { return a + b; }\n"
			"fun main() -> i64 = {\n"
			"    var total: i64 = add(1, 2);\n"
			"    return total;\n"
			"}\n"
		);

		assertTokenType(tokens, "add", TokenType::Function, 0);
		assertTokenType(tokens, "add", TokenType::Function, 1);
		assertTokenType(tokens, "main", TokenType::Function);
		assertTokenType(tokens, "a", TokenType::Parameter, 0);
		assertTokenType(tokens, "a", TokenType::Parameter, 1);
		assertTokenType(tokens, "b", TokenType::Parameter, 1);
		assertTokenType(tokens, "total", TokenType::Variable, 0);
		assertTokenType(tokens, "total", TokenType::Variable, 1);
	}

	void classMembersTest() {
		auto tokens = tokensOf(
			"lsp_sem_class_ws",
			"class Foo {\n"
			"    x: i64 = 0;\n"
			"    fun get() -> i64 = { return x; }\n"
			"}\n"
			"fun main() -> i64 = {\n"
			"    var foo = Foo(7);\n"
			"    return foo.x + foo.get();\n"
			"}\n"
		);

		assertTokenType(tokens, "Foo", TokenType::Class, 0);
		assertTokenType(tokens, "x", TokenType::Property, 0);
		assertTokenType(tokens, "get", TokenType::Method, 0);
		assertTokenType(tokens, "x", TokenType::Property, 2);
	}

	void constTypeTest() {
		auto tokens = tokensOf(
			"lsp_sem_const_ws",
			"const Num: type = i64;\n"
			"const limit = 10;\n"
			"fun main() -> Num = { return limit; }\n"
		);

		assertTokenType(tokens, "Num", TokenType::Type, 0);
		assertTokenType(tokens, "limit", TokenType::Variable, 0);
		assertTokenType(tokens, "Num", TokenType::Type, 1);
		assertTokenType(tokens, "limit", TokenType::Variable, 1);
	}

	void importedModuleTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_sem_import_ws");
		constexpr auto             CONTENT
			= "import helper;\n"
			  "fun main() -> i64 = { return helper.value(); }\n";
		workspace.add("pkg/pkg.dk", CONTENT)
			.add("pkg/helper.dk", "fun value() -> i64 = { return 1; }\n");
		const auto main_uri = workspace.uriOf("pkg/pkg.dk");

		under_test.compiler.addWorkspace(workspace.uriOf());
		under_test.compiler.openDocument(main_uri, "duckling", 1, CONTENT);

		auto tokens = under_test.compiler.semanticTokens(main_uri);
		assertTrue(tokens.has_value(), "An opened document must have semantic tokens");
		auto decoded = decode(tokens.value(), CONTENT);

		assertTokenType(decoded, "helper", TokenType::Namespace, 0);
		assertTokenType(decoded, "helper", TokenType::Namespace, 1);
		assertTokenType(decoded, "value", TokenType::Function);
	}

	void encodingTest() {
		constexpr auto CONTENT
			= "class Foo {\n"
			  "    x: i64 = 0;\n"
			  "    fun get(y: i64) -> i64 = { return x + y; }\n"
			  "}\n"
			  "fun main() -> i64 = { var foo = Foo(1); return foo.get(2); }\n";
		auto tokens = tokensOf("lsp_sem_encoding_ws", CONTENT);
		auto lines  = splitLines(CONTENT);

		assertTrue(!tokens.empty(), "The file must have tokens");
		for (usize i = 0; i < tokens.size(); i++) {
			const auto& token = tokens[i];
			assertTrue(token.length > 0, "A token must not be empty: " + token.text);
			assertTrue(
				token.character + token.length <= lines[token.line].size(),
				"A token must end on its own line: " + token.text
			);
			assertTrue(token.type < TokenType::MAX_VALUE, "A token must have a legend type");
			if (i == 0) continue;

			const auto& previous = tokens[i - 1];
			assertTrue(
				previous.line < token.line
					|| (previous.line == token.line
			            && previous.character + previous.length <= token.character),
				"Tokens must be sorted and must not overlap: " + previous.text + " then "
					+ token.text
			);
		}
	}

	void brokenPackageIsLexicalOnlyTest() {
		auto tokens = tokensOf(
			"lsp_sem_broken_ws",
			"fun main() -> i64 = { return 0; }\n"
			"fun broken( -> i64 = { return 1 }\n"
		);

		assertTokenType(tokens, "fun", TokenType::Keyword, 0);
		assertTokenType(tokens, "main", TokenType::Variable);
	}

	void followsEditsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_sem_edit_ws");
		constexpr auto             BEFORE = "fun main() -> i64 = { return 0; }\n";
		constexpr auto             AFTER  = "\nfun other() -> i64 = { return 0; }\n";
		workspace.add("pkg/pkg.dk", BEFORE);
		const auto main_uri = workspace.uriOf("pkg/pkg.dk");

		under_test.compiler.addWorkspace(workspace.uriOf());
		under_test.compiler.openDocument(main_uri, "duckling", 1, BEFORE);
		assertTrue(
			under_test.compiler
				.updateDocument(
					main_uri, 2, { lsp::TextDocumentContentChangeWholeDocument{ .text = AFTER } }
				)
				.isOk(),
			"The change must be applied"
		);

		auto tokens = under_test.compiler.semanticTokens(main_uri);
		assertTrue(tokens.has_value(), "An edited document must have semantic tokens");
		auto decoded = decode(tokens.value(), AFTER);

		assertTokenType(decoded, "other", TokenType::Function);
		assertTrue(decoded.front().line == 1, "The tokens must follow the edited buffer");
	}

	void unknownDocumentTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_sem_unknown_ws");
		assertTrue(
			under_test.compiler.semanticTokens(workspace.uriOf("nowhere/nothing.dk")).empty(),
			"A document outside of every package has no tokens"
		);
		assertTrue(
			under_test.compiler.semanticTokens(lsp::Uri::parse("untitled:Untitled-1")).empty(),
			"An unsupported URI has no tokens"
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
