#include <formatter/config.hpp>
#include <formatter/formatter.hpp>

#include <base/types/ints.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

	using formatter::FormatConfig;
	using formatter::IndentStyle;
	using lexer::Token;

	/** Tokenizes `source` and returns the formatter output. */
	std::string fmt(std::string_view source, const FormatConfig& config = FormatConfig::defaults()) {
		auto token_source
			= tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(source));
		token_source->tokenize();
		return formatter::formatTokens(token_source->getTokenData(), config, source);
	}

	using TokenSignature = std::vector<std::pair<i32, std::string>>;

	void collectSignature(const lexer::Tokens& tokens, TokenSignature& out) {
		for (const auto& token: tokens) {
			const auto type = token.getType();
			if (type == Token::Type::Empty || type == Token::Type::Sentinel) continue;
			out.emplace_back(static_cast<i32>(type), std::string(token.getStrValue()));
			if (type == Token::Type::BracketGroup || type == Token::Type::FormatString)
				collectSignature(token.getRecursive(), out);
		}
	}

	/** Flattened, whitespace-independent fingerprint of the significant tokens of `source`. */
	TokenSignature signatureOf(std::string_view source) {
		auto token_source
			= tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(source));
		token_source->tokenize();
		TokenSignature signature;
		collectSignature(token_source->getTokenData().tokens, signature);
		return signature;
	}

	std::string readFile(const std::string& path) {
		return std::string(fs::File(path).getContent().view().stringView());
	}
}

class FormatterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FormatterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// Golden output tests.
		TESTER_ADD_TEST(testEmpty);
		TESTER_ADD_TEST(testSimpleAssignment);
		TESTER_ADD_TEST(testOperatorSpacing);
		TESTER_ADD_TEST(testMemberAccess);
		TESTER_ADD_TEST(testTypeAnnotation);
		TESTER_ADD_TEST(testCallNormalization);
		TESTER_ADD_TEST(testIndexNormalization);
		TESTER_ADD_TEST(testAttribute);
		TESTER_ADD_TEST(testUnaryMinus);
		TESTER_ADD_TEST(testFunctionBlock);
		TESTER_ADD_TEST(testIfElseChain);
		TESTER_ADD_TEST(testWhileLoop);
		TESTER_ADD_TEST(testNestedBlocks);
		TESTER_ADD_TEST(testClassBody);
		TESTER_ADD_TEST(testEmptyBlockInline);
		TESTER_ADD_TEST(testCurlyLiteralInline);
		TESTER_ADD_TEST(testMultipleStatements);
		TESTER_ADD_TEST(testTrailingNewline);

		// Configuration-driven tests.
		TESTER_ADD_TEST(testSpaceIndent);
		TESTER_ADD_TEST(testNoSpaceAroundOperators);
		TESTER_ADD_TEST(testConfigFromJson);
		TESTER_ADD_TEST(testConfigDefaults);
		TESTER_ADD_TEST(testConfigUnknownKeysIgnored);

		// Property tests.
		TESTER_ADD_TEST(testRoundTripTokens);
		TESTER_ADD_TEST(testIdempotence);
		TESTER_ADD_TEST(testCommentsPreserved);
		TESTER_ADD_TEST(testOperatorAdjacency);
		TESTER_ADD_TEST(testRepoSnippets);
	}

private:
	/** Asserts `fmt(input) == expected`, printing both on failure. */
	void check(std::string_view input, std::string_view expected) {
		ASSERT_EQUAL_PRINT(std::string(expected), fmt(input));
	}

	void check(std::string_view input, std::string_view expected, const FormatConfig& config) {
		ASSERT_EQUAL_PRINT(std::string(expected), fmt(input, config));
	}

	void testEmpty() {
		ASSERT_EQUAL_PRINT(std::string(""), fmt(""));
		ASSERT_EQUAL_PRINT(std::string(""), fmt("   \n\t  \n"));
	}

	void testSimpleAssignment() {
		check("a=1;", "a = 1;\n");
		check("a   =   1 ;", "a = 1;\n");
	}

	void testOperatorSpacing() {
		check("a=b+c*d;", "a = b + c * d;\n");
		check("x=a<b;", "x = a < b;\n");
		check("x=a==b;", "x = a == b;\n");
	}

	void testMemberAccess() {
		check("a.b.c;", "a.b.c;\n");
		check("std . math . sqrt ;", "std.math.sqrt;\n");
	}

	void testTypeAnnotation() {
		check("x:i32;", "x: i32;\n");
		check("y : i64 = 5 ;", "y: i64 = 5;\n");
	}

	void testCallNormalization() {
		check("foo (a ,b );", "foo(a, b);\n");
		check("foo();", "foo();\n");
	}

	void testIndexNormalization() { check("arr [ i ] ;", "arr[i];\n"); }

	void testAttribute() { check("@Attr\nx=1;", "@Attr x = 1;\n"); }

	void testUnaryMinus() {
		// Note: `x=-1` lexes `=-` as a single operator, so a space is needed to get a unary minus.
		check("x = -1;", "x = -1;\n");
		check("x=a- -b;", "x = a - -b;\n");
		check("x=(-a);", "x = (-a);\n");
		check("return -1;", "return -1;\n");
	}

	void testFunctionBlock() {
		check("fun f()={x=1;y=2;}", "fun f() = {\n\tx = 1;\n\ty = 2;\n}\n");
	}

	void testIfElseChain() {
		check(
			"if(a){b;}else if(c){d;}else{e;}",
			"if (a) {\n\tb;\n} else if (c) {\n\td;\n} else {\n\te;\n}\n"
		);
	}

	void testWhileLoop() { check("while(a<b){c;}", "while (a < b) {\n\tc;\n}\n"); }

	void testNestedBlocks() {
		check("fun f()={if(a){b;}}", "fun f() = {\n\tif (a) {\n\t\tb;\n\t}\n}\n");
	}

	void testClassBody() {
		check(
			"class C{x:i32;fun m()={y=1;}}",
			"class C {\n\tx: i32;\n\tfun m() = {\n\t\ty = 1;\n\t}\n}\n"
		);
	}

	void testEmptyBlockInline() {
		check("block{}", "block {}\n");
		check("fun f()={}", "fun f() = {}\n");
	}

	void testCurlyLiteralInline() { check("x={1,2,3};", "x = {1, 2, 3};\n"); }

	void testMultipleStatements() { check("a=1;b=2;c=3;", "a = 1;\nb = 2;\nc = 3;\n"); }

	void testTrailingNewline() {
		const auto out = fmt("a=1;");
		assertTrue(!out.empty() && out.back() == '\n', "output must end with a newline");
		assertTrue(
			out.size() < 2 || out[out.size() - 2] != '\n', "output must end with a single newline"
		);
	}

	void testSpaceIndent() {
		FormatConfig config;
		config.indent_style = IndentStyle::Space;
		config.indent_width = 2;
		check("fun f()={x=1;}", "fun f() = {\n  x = 1;\n}\n", config);

		config.indent_width = 4;
		check("fun f()={x=1;}", "fun f() = {\n    x = 1;\n}\n", config);
	}

	void testNoSpaceAroundOperators() {
		FormatConfig config;
		config.space_around_operators = false;
		check("a = b + c;", "a=b+c;\n", config);
	}

	void testConfigFromJson() {
		const auto json   = nlohmann::json::parse(R"({
			"indentStyle": "space",
			"indentWidth": 2,
			"maxLineLength": 80,
			"spaceAroundOperators": false
		})");
		const auto config = FormatConfig::fromJson(json);
		ASSERT_EQUAL(IndentStyle::Space == config.indent_style, true);
		ASSERT_EQUAL(2u, config.indent_width);
		ASSERT_EQUAL(80u, config.max_line_length);
		ASSERT_EQUAL(false, config.space_around_operators);
	}

	void testConfigDefaults() {
		const auto config = FormatConfig::defaults();
		ASSERT_EQUAL(IndentStyle::Tab == config.indent_style, true);
		ASSERT_EQUAL(true, config.space_around_operators);
	}

	void testConfigUnknownKeysIgnored() {
		const auto json   = nlohmann::json::parse(R"({"unknownKey": 42, "indentWidth": 8})");
		const auto config = FormatConfig::fromJson(json);
		ASSERT_EQUAL(8u, config.indent_width);
		// Untouched keys keep their defaults.
		ASSERT_EQUAL(IndentStyle::Tab == config.indent_style, true);
	}

	/** Formatting must never change the significant token stream of a program. */
	void testRoundTripTokens() {
		const std::vector<std::string_view> samples = {
			"a=1;",
			"a=b+c*d-e/f;",
			"foo(a,b,c);",
			"x:i32;y:i64=5;",
			"a.b.c.d;",
			"if(a){b;}else if(c){d;}else{e;}",
			"while(a<b){c=c+1;}",
			"fun f(a:i32,b:i64)->i64={return a+b;}",
			"class C{x:i32;private y:i64=5;fun m()={z=1;}}",
			"x={1,2,3};",
			"block{}",
			"x=-1;y=a- -b;z=!c;",
			"@Attr fun g()={h();}",
			"a=arr[i+1];",
		};
		for (const auto& sample: samples) {
			const auto before = signatureOf(sample);
			const auto after  = signatureOf(fmt(sample));
			assertTrue(
				before == after, base::strConcat("formatting changed token stream for: ", sample)
			);
		}
	}

	/** Formatting a formatted program must be a no-op. */
	void testIdempotence() {
		const std::vector<std::string_view> samples = {
			"a=1;",
			"if(a){b;}else{c;}",
			"class C{x:i32;fun m()={y=1;}}",
			"fun f(a:i32)->i64={return a;}",
			"x={1,2,3};",
			"while(a<b){c;}",
		};
		for (const auto& sample: samples) {
			const auto once  = fmt(sample);
			const auto twice = fmt(once);
			ASSERT_EQUAL_PRINT(once, twice);
		}
	}

	void testCommentsPreserved() {
		const std::vector<std::string_view> samples = {
			"a=1;//trailing\nb=2;",
			"//leading\nx=1;",
			"fun f()={//inner\nx=1;}",
		};
		for (const auto& sample: samples) {
			const auto before = signatureOf(sample);
			const auto after  = signatureOf(fmt(sample));
			assertTrue(before == after, base::strConcat("comment lost while formatting: ", sample));
			// And idempotent.
			ASSERT_EQUAL_PRINT(fmt(sample), fmt(fmt(sample)));
		}
	}

	/** Adjacent operator tokens must never be merged by the formatter (the lexer is greedy). */
	void testOperatorAdjacency() {
		const std::vector<std::string_view> samples = {
			"x = a ** b . c -> d;",
			"x = a + + b;",
			"x = a - -b;",
			"x = * * .;",
		};
		for (const auto& sample: samples) {
			const auto before = signatureOf(sample);
			const auto after  = signatureOf(fmt(sample));
			assertTrue(
				before == after, base::strConcat("operator adjacency merged tokens for: ", sample)
			);
		}

		// Even with spaces disabled, operators that would merge must stay separated.
		FormatConfig tight;
		tight.space_around_operators                      = false;
		const std::vector<std::string_view> tight_samples = { "a - -b;", "a ** b;", "a + +b;" };
		for (const auto& sample: tight_samples) {
			const auto before = signatureOf(sample);
			const auto after  = signatureOf(fmt(sample, tight));
			assertTrue(
				before == after,
				base::strConcat("tight-mode operator adjacency merged tokens for: ", sample)
			);
		}
	}

	/** Real Duckling snippets copied from across the repository must round-trip and be idempotent. */
	void testRepoSnippets() {
		const std::vector<std::string_view> snippets = {
			"actions",
			"block",
			"class",
			"expressions",
			"ffi",
			"for",
			"fun",
			"fun2",
			"function_with_parameters",
			"if",
			"import",
			"lists_ok",
			"namespace",
			"numeric_literals",
			"pattern",
			"using",
			"while",
			"repl_loops",
			"playground_main",
			"playground_mod",
			"format_strings",
		};
		for (const auto& name: snippets) {
			const auto source = readFile(path(base::strConcat("snippets/", name, ".duck")));

			const auto before = signatureOf(source);
			const auto after  = signatureOf(fmt(source));
			assertTrue(
				before == after,
				base::strConcat("formatting changed token stream of snippet: ", name)
			);

			const auto once = fmt(source);
			ASSERT_EQUAL_PRINT(once, fmt(once));
		}
	}

public:
	~FormatterTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/formatter/tests/");
