// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/diagnostic_state.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

#include <array>
#include <sstream>
#include <string_view>
#include <utility>

class LexerErrorTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LexerErrorTests

	struct GenExample;
	static std::vector<GenExample*> examples;

	struct GenExample {
		std::string code;

		GenExample(std::string code): code(std::move(code)) { examples.push_back(this); }

		virtual bool operator()() = 0;
		[[nodiscard]]
		virtual std::string message() const
			= 0;

		virtual ~GenExample() = default;
	};

	template<bool good = true>
	struct Example: public GenExample {
		Example(std::string code): GenExample(std::move(code)) {}

		bool operator()() override {
			auto file = tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(code));
			return file->tokenize() == good;
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while lexing: `" << code << "`";
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<bool good>
	void testExample(Example<good>& example) {
		assertTrue(example(), example.message());
	}

	std::array<std::unique_ptr<GenExample>, 10> unclosed_eof = {
		std::make_unique<Example<false>>("("),
		std::make_unique<Example<false>>("{"),
		std::make_unique<Example<false>>("["),
		std::make_unique<Example<false>>("\""),
		std::make_unique<Example<false>>("'"),
		std::make_unique<Example<false>>("#{"),
		std::make_unique<Example<false>>("#tag{"),
		std::make_unique<Example<false>>("#tag{#}"),
		std::make_unique<Example<false>>("#tag{#other}"),
		std::make_unique<Example<false>>("#tag{#ta"),
	};

	std::array<std::unique_ptr<GenExample>, 2> unclosed_eol = {
		std::make_unique<Example<false>>("\"\n"),
		std::make_unique<Example<false>>("'\n"),
	};

	// Literals ending in a backslash right before EOF must produce an
	// unclosed-literal diagnostic instead of crashing the lexer.
	std::array<std::unique_ptr<GenExample>, 3> trailing_backslash_eof = {
		std::make_unique<Example<false>>(R"("abc\)"),
		std::make_unique<Example<false>>(R"('a\)"),
		std::make_unique<Example<false>>(R"(f"abc\)"),
	};

	std::array<std::unique_ptr<GenExample>, 2> bad_char_count = {
		std::make_unique<Example<false>>("''"),
		std::make_unique<Example<false>>("'aaaa'"),
	};

	std::array<std::unique_ptr<GenExample>, 4> good_char = {
		std::make_unique<Example<true>>("'a'"),
		std::make_unique<Example<true>>("'z'"),
		std::make_unique<Example<true>>("'+'"),
		std::make_unique<Example<true>>("'\\n'"),
	};

	std::array<std::unique_ptr<GenExample>, 4> bad_fmt_str = {
		std::make_unique<Example<false>>(R"(f")"),
		std::make_unique<Example<false>>(R"(f"\n)"),
		std::make_unique<Example<false>>(R"(f"{")"),
		std::make_unique<Example<false>>(R"(f"{}x)"),
	};

	std::array<std::unique_ptr<GenExample>, 1> bad_type_specifier
		= { std::make_unique<Example<false>>("123abc") };

	Example<false> bad_char_start{ "\xCC\x80" };

	void exampleTests() {
		for (auto e: examples) assertTrue((*e)(), e->message());
	}

	void testUnclosedCommentClosingDelimiter() {
		const std::array<std::pair<std::string_view, std::string_view>, 5> cases = { {
			{ "#{", "#}" },
			{ "#tag{#}", "#tag}" },
			{ "#Tag{#tag}", "#Tag}" },
			{ "#zażółć{#}", "#zażółć}" },
			{ "#a\u0301{#á}", "#a\u0301}" },
		} };
		for (const auto& [code, closing_delimiter]: cases) {
			auto source
				= tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(code));
			ASSERT_TRUE(!source->tokenize());
			std::vector<CRef<dia::dia_args::Diagnostic>> diagnostics;
			source->getIntLogger()->collectDiagnostics(diagnostics);
			ASSERT_EQUAL(diagnostics.size(), 1);
			auto       diagnostic = dia::evaluateDiagnostic(*diagnostics.front());
			const auto header     = dia::constructTextView(diagnostic.messages[0].header.ref());
			ASSERT_EQUAL(
				header,
				base::strConcat(
					"Unclosed block comment starting here. Expected closing delimiter `",
					closing_delimiter,
					"`."
				)
			);
		}
	}

	void testUnclosedNamedCommentPosition() {
		auto source = tokenizer::makeTokenSource(
			fs::FileManager::createRandomVirtualFile("\n  #tag{comment #other}")
		);
		ASSERT_TRUE(!source->tokenize());
		std::vector<CRef<dia::dia_args::Diagnostic>> diagnostics;
		source->getIntLogger()->collectDiagnostics(diagnostics);
		ASSERT_EQUAL(diagnostics.size(), 1);
		const auto& message = diagnostics.front()->main_message;
		ASSERT_EQUAL(message.metadata.name, "unclosed_comment_error");
		const auto& component = dynamic_cast<const dia::dia_args::CodeLocationComponent&>(
			*message.arguments.at("code_location")
		);
		const auto& location = component.location;
		ASSERT_EQUAL(location.line, 2);
		ASSERT_EQUAL(location.column, 3);
		ASSERT_TRUE(location.end_line && *location.end_line == 2);
		ASSERT_TRUE(location.end_column && *location.end_column == 7);
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(exampleTests);
		TESTER_ADD_TEST(testUnclosedCommentClosingDelimiter);
		TESTER_ADD_TEST(testUnclosedNamedCommentPosition);
	}

public:
	~LexerErrorTests() override = default;
};

std::vector<LexerErrorTests::GenExample*> LexerErrorTests::examples = {};

TESTER_COMMON_MAIN("/src/compiler/tokenizer/lexer/tests");
