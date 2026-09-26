#include <formatter/config.hpp>
#include <formatter/formatter.hpp>

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

	using formatter::FormatConfig;
	using formatter::IndentStyle;
	using lexer::Token;

	/** Tokenizes `source` (keeping comments) and returns the formatter output. */
	std::string fmt(std::string_view source, const FormatConfig& config = FormatConfig::defaults()) {
		auto token_source
			= tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(source));
		token_source->tokenize(/*keep_comments=*/true);
		return formatter::formatTokens(token_source->getTokenData(), config);
	}

	/**
	 * Strips the newline right after the opening `R"(` so golden outputs can be written as
	 * readable multi-line raw string literals starting on their own line.
	 */
	std::string_view golden(std::string_view text) {
		if (!text.empty() && text.front() == '\n') text.remove_prefix(1);
		return text;
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
		token_source->tokenize(/*keep_comments=*/true);
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
		TESTER_ADD_TEST(testImports);
		TESTER_ADD_TEST(testArrayTypes);
		TESTER_ADD_TEST(testAttribute);
		TESTER_ADD_TEST(testUnaryMinus);
		TESTER_ADD_TEST(testUnaryAddressOfAndOptional);
		TESTER_ADD_TEST(testIncrementDecrement);
		TESTER_ADD_TEST(testMatchArms);
		TESTER_ADD_TEST(testFunctionBlock);
		TESTER_ADD_TEST(testBraceBlockWithoutSemicolon);
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

		// New syntax.
		TESTER_ADD_TEST(testTemplateHeader);
		TESTER_ADD_TEST(testGenericInstantiation);
		TESTER_ADD_TEST(testCustomOperatorDeclaration);
		TESTER_ADD_TEST(testTextOperatorsKeepSpacing);
		TESTER_ADD_TEST(testVariantTypeSpacing);
		TESTER_ADD_TEST(testBitwiseOperatorSpacing);

		// Comment placement.
		TESTER_ADD_TEST(testCommentAfterBlock);
		TESTER_ADD_TEST(testTrailingBlockComment);
		TESTER_ADD_TEST(testCommentEndingGroupElement);
		TESTER_ADD_TEST(testMultiLineBlockComment);

		// Column measurement and robustness.
		TESTER_ADD_TEST(testUnicodeColumnsMeasuredVisually);
		TESTER_ADD_TEST(testInvalidIndentStyleRejected);
		TESTER_ADD_TEST(testNestingTooDeep);

		// Line-wrapping tests: bracket groups.
		TESTER_ADD_TEST(testWrapLongCall);
		TESTER_ADD_TEST(testWrapLongSignature);
		TESTER_ADD_TEST(testWrapNested);
		TESTER_ADD_TEST(testWrapAssignedList);
		TESTER_ADD_TEST(testNoWrapWhenFits);
		TESTER_ADD_TEST(testNoWrapUnbreakable);

		// Line-wrapping tests: expressions.
		TESTER_ADD_TEST(testWrapLongBinaryExpression);
		TESTER_ADD_TEST(testWrapChainedCalls);
		TESTER_ADD_TEST(testWrapChainWithArguments);
		TESTER_ADD_TEST(testWrapDeepIndentation);
		TESTER_ADD_TEST(testWrapRoundTripAndIdempotent);

		// Comment tests.
		TESTER_ADD_TEST(testWrapLongComment);
		TESTER_ADD_TEST(testWrapDocCommentPrefix);
		TESTER_ADD_TEST(testWrapCommentInBlock);
		TESTER_ADD_TEST(testNoCommentWrapWhenFits);
		TESTER_ADD_TEST(testTrailingComment);
		TESTER_ADD_TEST(testBlockCommentInline);
		TESTER_ADD_TEST(testCommentWrapIdempotent);
		TESTER_ADD_TEST(testMixedCommentPrefixReflow);
		TESTER_ADD_TEST(testTrailingCommentReflow);

		// Comments inside bracket groups.
		TESTER_ADD_TEST(testCommentInExplodedGroup);
		TESTER_ADD_TEST(testStandaloneCommentInGroup);
		TESTER_ADD_TEST(testCommentForcesExplosion);
		TESTER_ADD_TEST(testCommentInCommalessGroup);
		TESTER_ADD_TEST(testBlockCommentInGroupStaysFlat);
		TESTER_ADD_TEST(testDeepNestingStress);

		// Empty line tests.
		TESTER_ADD_TEST(testEmptyLinesPreserved);
		TESTER_ADD_TEST(testEmptyLinesCapped);
		TESTER_ADD_TEST(testEmptyLinesConfigured);
		TESTER_ADD_TEST(testEmptyLinesIdempotent);
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
		check("a .? b;", "a.?b;\n");
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

	void testImports() {
		check("import a.b.c.d;", "import a.b.c.d;\n");
		check("import a.b.c as D;", "import a.b.c as D;\n");
		check("import a.b .* ;", "import a.b.*;\n");
		check("import A.B.(C, D);", "import A.B.(C, D);\n");
		check("import A.B .* hides D ,G, C;", "import A.B.* hides D, G, C;\n");
	}

	void testArrayTypes() {
		// Built-in type keywords bind tightly to their array/generic group.
		check("var data : i32 [5] = [1, 2];", "var data: i32[5] = [1, 2];\n");
		check("var l : List [i32] = [];", "var l: List[i32] = [];\n");
		// Custom types and parenthesized types do too.
		check("var p : MyClass [3] = [a, b];", "var p: MyClass[3] = [a, b];\n");
		check("var g : (box Greeter) [2] = [x, y];", "var g: (box Greeter)[2] = [x, y];\n");
		// Generic declarations use square brackets after the name.
		check("fun grab [T] (value : T) -> Holder [T] ;", "fun grab[T](value: T) -> Holder[T];\n");
		// Non-type keywords keep their separating space.
		check("return [1, 2];", "return [1, 2];\n");
	}

	void testAttribute() { check("@Attr\nx=1;", "@Attr x = 1;\n"); }

	void testUnaryMinus() {
		// Note: `x=-1` lexes `=-` as a single operator, so a space is needed to get a unary minus.
		check("x = -1;", "x = -1;\n");
		check("x=a- -b;", "x = a - -b;\n");
		check("x=(-a);", "x = (-a);\n");
		check("return -1;", "return -1;\n");
	}

	void testUnaryAddressOfAndOptional() {
		check("x = &y;", "x = &y;\n");
		check("f(&a, b & c);", "f(&a, b & c);\n");
		check(
			"pattern T(b: ref B) -> ref D = &b.data;", "pattern T(b: ref B) -> ref D = &b.data;\n"
		);
		// `?` is a prefix type operator: `?i32` is an optional i32.
		check("pattern S(r: ref R) -> ?i32 = none;", "pattern S(r: ref R) -> ?i32 = none;\n");
	}

	void testIncrementDecrement() {
		check("t++;", "t++;\n");
		check("--t;", "--t;\n");
		check("arr[i]++;", "arr[i]++;\n");
	}

	void testMatchArms() {
		// Match arms carry no trailing `;`; each `case` still starts its own line.
		check("match (rect) {case Square(len) = print(a) case _ = print(b)}", golden(R"(
match (rect) {
	case Square(len) = print(a)
	case _ = print(b)
}
)"));
	}

	void testFunctionBlock() {
		check("fun f()={x=1;y=2;}", golden(R"(
fun f() = {
	x = 1;
	y = 2;
}
)"));
	}

	void testBraceBlockWithoutSemicolon() {
		// A single-statement block with no trailing `;` is still a code block: it must explode
		// onto its own lines, and the statement after it must not glue to the closing brace.
		check("fun fib() = {if (n < 2) {return n} return f(n)}", golden(R"(
fun fib() = {
	if (n < 2) {
		return n
	}
	return f(n)
}
)"));
		// A curly with comma-separated expressions and no statement keyword stays an inline literal.
		check("let s = {1, 2, 3};", "let s = {1, 2, 3};\n");
	}

	void testIfElseChain() {
		check("if(a){b;}else if(c){d;}else{e;}", golden(R"(
if (a) {
	b;
} else if (c) {
	d;
} else {
	e;
}
)"));
	}

	void testWhileLoop() {
		check("while(a<b){c;}", golden(R"(
while (a < b) {
	c;
}
)"));
	}

	void testNestedBlocks() {
		check("fun f()={if(a){b;}}", golden(R"(
fun f() = {
	if (a) {
		b;
	}
}
)"));
	}

	void testClassBody() {
		check("class C{x:i32;fun m()={y=1;}}", golden(R"(
class C {
	x: i32;
	fun m() = {
		y = 1;
	}
}
)"));
	}

	void testEmptyBlockInline() {
		check("block{}", "block {}\n");
		check("fun f()={}", "fun f() = {}\n");
	}

	void testCurlyLiteralInline() { check("x={1,2,3};", "x = {1, 2, 3};\n"); }

	void testMultipleStatements() {
		check("a=1;b=2;c=3;", golden(R"(
a = 1;
b = 2;
c = 3;
)"));
	}

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
		check(
			"fun f()={x=1;}",
			golden(R"(
fun f() = {
  x = 1;
}
)"),
			config
		);

		config.indent_width = 4;
		check(
			"fun f()={x=1;}",
			golden(R"(
fun f() = {
    x = 1;
}
)"),
			config
		);
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
			"maxEmptyLines": 1,
			"spaceAroundOperators": false
		})");
		const auto config = FormatConfig::fromJson(json);
		ASSERT_EQUAL(IndentStyle::Space == config.indent_style, true);
		ASSERT_EQUAL(2u, config.indent_width);
		ASSERT_EQUAL(80u, config.max_line_length);
		ASSERT_EQUAL(1u, config.max_empty_lines);
		ASSERT_EQUAL(false, config.space_around_operators);
	}

	void testConfigDefaults() {
		const auto config = FormatConfig::defaults();
		ASSERT_EQUAL(IndentStyle::Tab == config.indent_style, true);
		ASSERT_EQUAL(2u, config.max_empty_lines);
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
			"a=1;# trailing\nb=2;",
			"# leading\nx=1;",
			"fun f()={# inner\nx=1;}",
			"x = #{ inline block comment #} 1;",
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
			// A sign operator asking to glue must not swallow the operator after it.
			"x = ++ * y;",
			"z = -- ++: w;",
			// `not` is spelled with letters: gluing it would merge it into an identifier.
			"x = not b;",
			"if (not x) { y; }",
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
		// Read the directory rather than a hand-written list, so a snippet added later is
		// covered automatically.
		std::vector<std::string> snippets;
		for (const auto& entry: std::filesystem::directory_iterator(path("snippets")))
			if (entry.path().extension() == ".duck") snippets.push_back(entry.path().string());
		std::ranges::sort(snippets);
		assertTrue(!snippets.empty(), "no snippets found");

		for (const auto& snippet: snippets) {
			const auto source = readFile(snippet);

			const auto before = signatureOf(source);
			const auto after  = signatureOf(fmt(source));
			assertTrue(
				before == after,
				base::strConcat("formatting changed token stream of snippet: ", snippet)
			);

			const auto once = fmt(source);
			ASSERT_EQUAL_PRINT(once, fmt(once));
		}
	}

	// ===== New syntax =====

	void testTemplateHeader() {
		check("template(T: type) fun pick() -> i64 = {return 1;}", golden(R"(
template(T: type)
fun pick() -> i64 = {
	return 1;
}
)"));
		// Also in front of a plain declaration.
		check("template(a: i64) const C: i64 = a;", "template(a: i64)\nconst C: i64 = a;\n");
	}

	void testGenericInstantiation() {
		check("x = impl.max : {i32}(a, b);", "x = impl.max:{i32}(a, b);\n");
		check("x = pick:{char}();", "x = pick:{char}();\n");
	}

	void testCustomOperatorDeclaration() {
		check("fun +* (a: i64, b: i64) -> i64 = a * b;", "fun +*(a: i64, b: i64) -> i64 = a * b;\n");
		check("fun -*(a: i64) -> i64 = a;", "fun -*(a: i64) -> i64 = a;\n");
		// A binary operator in an expression keeps its spaces.
		check("x = a + (b);", "x = a + (b);\n");
	}

	void testTextOperatorsKeepSpacing() {
		check("x = not b;", "x = not b;\n");
		check("x = not (b);", "x = not (b);\n");
		check("y = copyof x;", "y = copyof x;\n");
		check("z = ptrof x;", "z = ptrof x;\n");
	}

	void testVariantTypeSpacing() {
		check("fun f(v: i32 | f32) -> i64 = 0i64;", "fun f(v: i32 | f32) -> i64 = 0i64;\n");
	}

	void testBitwiseOperatorSpacing() {
		check("x = ~a | b & 3 ^ 1 << 2 >> 1;", "x = ~a | b & 3 ^ 1 << 2 >> 1;\n");
	}

	// ===== Comment placement =====

	void testCommentAfterBlock() {
		check("fun f() = {\n\tx = 1;\n} # done\n", golden(R"(
fun f() = {
	x = 1;
} # done
)"));
	}

	void testTrailingBlockComment() { check("x = 1; #{ c #}\n", "x = 1; #{ c #}\n"); }

	/** A comment closing a group element must not leave a line holding only indentation. */
	void testCommentEndingGroupElement() {
		check("foo(\n\taaa\n\t# why\n);\n", golden(R"(
foo(
	aaa # why
);
)"));
		const auto once = fmt("foo(\n\taaa\n\t# why\n);\n");
		ASSERT_EQUAL_PRINT(once, fmt(once));
	}

	/**
	 * A multi-line block comment ends its own line: what follows is measured from the start of
	 * a fresh line, not from the comment's total length.
	 */
	void testMultiLineBlockComment() {
		const std::string comment = "#{ " + std::string(95, 'a') + "\nbb #}";
		const auto        out     = fmt(comment + " f(1, 2, 3);");
		assertTrue(
			out.find("f(1, 2, 3)") != std::string::npos,
			base::strConcat("group exploded after a multi-line block comment: ", out)
		);
	}

	// ===== Column measurement =====

	/** Columns count characters, not bytes, so non-ASCII text wraps where an editor shows it. */
	void testUnicodeColumnsMeasuredVisually() {
		const auto config = narrowConfig(40);
		// 36 visual columns, but 60 bytes.
		check(
			"foo(\u1f04\u03bb\u03c6\u03b1\u1f04\u03bb\u03c6\u03b1, "
			"\u03b2\u1fc6\u03c4\u03b1\u03b2\u1fc6\u03c4\u03b1, "
			"\u03b3\u03ac\u03bc\u03bc\u03b1\u03b3\u03ac\u03bc\u03bc\u03b1);",
			"foo(\u1f04\u03bb\u03c6\u03b1\u1f04\u03bb\u03c6\u03b1, "
			"\u03b2\u1fc6\u03c4\u03b1\u03b2\u1fc6\u03c4\u03b1, "
			"\u03b3\u03ac\u03bc\u03bc\u03b1\u03b3\u03ac\u03bc\u03bc\u03b1);\n",
			config
		);
	}

	// ===== Robustness =====

	void testInvalidIndentStyleRejected() {
		assertThrows<nlohmann::json::exception>(
			[] {
				const auto json = nlohmann::json::parse(R"({"indentStyle": "tabs"})");
				return FormatConfig::fromJson(json);
			},
			"a misspelled indentStyle must be rejected, not silently defaulted"
		);
		assertThrows<nlohmann::json::exception>(
			[] {
				const auto json = nlohmann::json::parse(R"({"indentStyle": 7})");
				return FormatConfig::fromJson(json);
			},
			"a non-string indentStyle must be rejected"
		);
	}

	/** Pathological nesting is reported, not crashed on. */
	void testNestingTooDeep() {
		const usize depth  = 5'000;
		std::string source = "x = " + std::string(depth, '(') + "1" + std::string(depth, ')') + ";";
		assertThrows<base::LogicError>(
			[&source] { return fmt(source); }, "deep nesting must throw, not overflow the stack"
		);
	}

	static FormatConfig narrowConfig(u32 width) {
		FormatConfig config;
		config.max_line_length = width;
		return config;
	}

	void testWrapLongCall() {
		check(
			"foo(aaaa, bbbb, cccc);",
			golden(R"(
foo(
	aaaa,
	bbbb,
	cccc
);
)"),
			narrowConfig(20)
		);
	}

	void testWrapLongSignature() {
		check(
			"fun f(aaaa: i32, bbbb: i32) = {x=1;}",
			golden(R"(
fun f(
	aaaa: i32,
	bbbb: i32
) = {
	x = 1;
}
)"),
			narrowConfig(20)
		);
	}

	void testWrapNested() {
		// The inner call exceeds the width too, so it explodes one level deeper.
		check(
			"foo(bar(aaaa, bbbb, cccc), ddddddd);",
			golden(R"(
foo(
	bar(
		aaaa,
		bbbb,
		cccc
	),
	ddddddd
);
)"),
			narrowConfig(20)
		);
	}

	void testWrapAssignedList() {
		// The list explodes; the assignment itself stays on the opening line.
		check(
			"x = [aaaa, bbbb, cccc, dddd, eeee];",
			golden(R"(
x = [
	aaaa,
	bbbb,
	cccc,
	dddd,
	eeee
];
)"),
			narrowConfig(24)
		);
	}

	void testNoWrapWhenFits() {
		// Comfortably under the default 100-column limit: stays on one line.
		check("foo(a, b, c);", "foo(a, b, c);\n");
	}

	void testNoWrapUnbreakable() {
		// No top-level comma to break on, so an over-long group stays inline.
		check("foo(reallyLongSingleArgument);", "foo(reallyLongSingleArgument);\n", narrowConfig(5));
	}

	void testWrapLongBinaryExpression() {
		// No group to explode, so the expression breaks before binary operators, greedily
		// filling each line.
		check(
			"x = aaaaaa + bbbbbb + cccccc + dddddd;",
			golden(R"(
x = aaaaaa + bbbbbb
	+ cccccc + dddddd;
)"),
			narrowConfig(24)
		);
	}

	void testWrapChainedCalls() {
		// A method chain breaks before each `.` that follows a call.
		check(
			"value.foo(aa).bar(bb).baz(cc);",
			golden(R"(
value.foo(aa)
	.bar(bb)
	.baz(cc);
)"),
			narrowConfig(16)
		);
	}

	void testWrapChainWithArguments() {
		// The first call explodes its arguments; the rest of the chain fits after the
		// closing bracket.
		check(
			"obj.fetch(aaaa, bbbb, cccc).map(x).run();",
			golden(R"(
obj.fetch(
	aaaa,
	bbbb,
	cccc
).map(x).run();
)"),
			narrowConfig(20)
		);
	}

	void testWrapDeepIndentation() {
		// Wrapping respects the indentation of deeply nested blocks.
		check(
			"fun f() = {if (a) {while (b) {result = foo(aaaa, bbbb, cccc);}}}",
			golden(R"(
fun f() = {
	if (a) {
		while (b) {
			result = foo(
				aaaa,
				bbbb,
				cccc
			);
		}
	}
}
)"),
			narrowConfig(28)
		);
	}

	/** Every wrapped form must keep the token stream intact and be stable under re-formatting. */
	void testWrapRoundTripAndIdempotent() {
		const auto                          config  = narrowConfig(24);
		const std::vector<std::string_view> samples = {
			"foo(aaaa, bbbb, cccc, dddd);",
			"fun f(aaaa: i32, bbbb: i32, cccc: i32) = {return aaaa;}",
			"foo(bar(aaaa, bbbb, cccc), ddddddd, eeeeeee);",
			"x = [aaaa, bbbb, cccc, dddd, eeee];",
			"x = aaaaaa + bbbbbb + cccccc + dddddd + eeeeee;",
			"value.foo(aa).bar(bb).baz(cc).qux(dd);",
			"obj.fetch(aaaa, bbbb, cccc).map(x).filter(y).run();",
			"fun f() = {if (a) {result = foo(aaaa, bbbb) + bar(cccc, dddd);}}",
			"total = first(aaaa, bbbb) + second(cccc, dddd) * third(eeee);",
		};
		for (const auto& sample: samples) {
			const auto before = signatureOf(sample);
			const auto after  = signatureOf(fmt(sample, config));
			assertTrue(
				before == after, base::strConcat("wrapping changed token stream for: ", sample)
			);
			const auto once = fmt(sample, config);
			ASSERT_EQUAL_PRINT(once, fmt(once, config));
		}
	}

	void testWrapLongComment() {
		check(
			"# this is a very long comment that definitely exceeds the configured maximum "
			"line length",
			golden(R"(
# this is a very long comment that
# definitely exceeds the configured
# maximum line length
)"),
			narrowConfig(40)
		);
	}

	void testWrapDocCommentPrefix() {
		// Continuation lines repeat the full multi-`#` prefix.
		check(
			"## returns the sum of all elements in the given list",
			golden(R"(
## returns the sum of all
## elements in the given list
)"),
			narrowConfig(30)
		);
	}

	void testWrapCommentInBlock() {
		// Continuation lines keep the indentation of the comment. The tab indent counts as
		// indent_width visual columns, so the wrap point honors what an editor shows.
		check(
			"fun f() = {# explanation of the tricky part of this code\nx = 1;}",
			golden(R"(
fun f() = {
	# explanation of the
	# tricky part of this code
	x = 1;
}
)"),
			narrowConfig(30)
		);
	}

	void testNoCommentWrapWhenFits() {
		// A fitting comment is reproduced verbatim, inner spacing included.
		check("#  keep   inner   spacing", "#  keep   inner   spacing\n");
	}

	void testTrailingComment() {
		// A comment trailing a statement on the same source line stays on that line; a
		// comment on its own line stays on its own line.
		check("a=1;# note\nb=2;", golden(R"(
a = 1; # note
b = 2;
)"));
		check("a=1;\n# standalone\nb=2;", golden(R"(
a = 1;
# standalone
b = 2;
)"));
	}

	void testBlockCommentInline() {
		// A `#{ ... #}` comment is reproduced verbatim and does not end the statement.
		check("x =   #{ inline note #}   1;", "x = #{ inline note #} 1;\n");
	}

	void testEmptyLinesPreserved() {
		// Empty lines between statements survive formatting, inside blocks too.
		check("a=1;\n\nb=2;", golden(R"(
a = 1;

b = 2;
)"));
		check("fun f()={x=1;\n\ny=2;}", golden(R"(
fun f() = {
	x = 1;

	y = 2;
}
)"));
	}

	void testEmptyLinesCapped() {
		// More than max_empty_lines (default 2) consecutive empty lines collapse to the limit.
		check("a=1;\n\n\n\n\n\nb=2;", golden(R"(
a = 1;


b = 2;
)"));
		// Leading and trailing empty lines are always dropped.
		check("\n\n\na=1;\n\n\n", "a = 1;\n");
	}

	void testEmptyLinesConfigured() {
		FormatConfig config;
		config.max_empty_lines = 0;
		check("a=1;\n\n\nb=2;", "a = 1;\nb = 2;\n", config);

		config.max_empty_lines = 1;
		check("a=1;\n\n\nb=2;", "a = 1;\n\nb = 2;\n", config);
	}

	void testEmptyLinesIdempotent() {
		const std::vector<std::string_view> samples = {
			"a=1;\n\nb=2;",
			"a=1;\n\n\n\n\nb=2;\n\nc=3;",
			"fun f()={x=1;\n\n\n\ny=2;}",
			"a=1; # note\n\nb=2;",
		};
		for (const auto& sample: samples) {
			const auto once = fmt(sample);
			ASSERT_EQUAL_PRINT(once, fmt(once));
		}
	}

	void testMixedCommentPrefixReflow() {
		// Continuation lines repeat the full prefix whatever its length.
		check(
			"### three hash comment that is long enough to need reflowing",
			golden(R"(
### three hash comment that is
### long enough to need
### reflowing
)"),
			narrowConfig(30)
		);
	}

	void testTrailingCommentReflow() {
		// An over-long comment trailing a statement re-flows; continuations start at the
		// statement's indentation.
		check(
			"a = 1; # a trailing comment so long that it does not fit on the statement line",
			golden(R"(
a = 1; # a trailing comment so long that
# it does not fit on the statement line
)"),
			narrowConfig(40)
		);
	}

	/** A line comment between arguments stays with its element when the group explodes. */
	void testCommentInExplodedGroup() {
		const std::string_view source = "foo(aaaa, # first\nbbbb, cccc);";
		check(
			source,
			golden(R"(
foo(
	aaaa, # first
	bbbb,
	cccc
);
)"),
			narrowConfig(20)
		);

		// The comment must not swallow the following element on re-tokenization.
		ASSERT_EQUAL(signatureOf(source) == signatureOf(fmt(source, narrowConfig(20))), true);
		const auto once = fmt(source, narrowConfig(20));
		ASSERT_EQUAL_PRINT(once, fmt(once, narrowConfig(20)));
	}

	void testStandaloneCommentInGroup() {
		// A comment on its own source line keeps its own line inside the exploded group.
		check("foo(aaaa,\n# pick wisely\nbbbb);", golden(R"(
foo(
	aaaa,
	# pick wisely
	bbbb
);
)"));
	}

	void testCommentForcesExplosion() {
		// The group fits the default line length, but a line comment can never render inline.
		check("foo(a, # note\nb);", golden(R"(
foo(
	a, # note
	b
);
)"));
	}

	void testCommentInCommalessGroup() {
		const std::string_view source = "foo(aaaa # why\n);";
		check(source, golden(R"(
foo(
	aaaa # why
);
)"));
		ASSERT_EQUAL(signatureOf(source) == signatureOf(fmt(source)), true);
	}

	void testBlockCommentInGroupStaysFlat() {
		// A `#{ ... #}` comment does not end its line, so the group stays inline.
		check("foo(a, #{ ok #} b);", "foo(a, #{ ok #} b);\n");
	}

	void testDeepNestingStress() {
		const auto             config = narrowConfig(20);
		const std::string_view source = "x = f(g(h(aaaa, bbbb), cccc), dddd);";
		check(
			source,
			golden(R"(
x = f(
	g(
		h(
			aaaa,
			bbbb
		),
		cccc
	),
	dddd
);
)"),
			config
		);
		ASSERT_EQUAL(signatureOf(source) == signatureOf(fmt(source, config)), true);
		const auto once = fmt(source, config);
		ASSERT_EQUAL_PRINT(once, fmt(once, config));
	}

	void testCommentWrapIdempotent() {
		const auto                          config  = narrowConfig(40);
		const std::vector<std::string_view> samples = {
			"# this is a very long comment that definitely exceeds the configured maximum "
			"line length",
			"## a long documentation comment that needs to be wrapped onto several lines",
			"fun f() = {# a long comment inside a block that needs wrapping to fit\nx = 1;}",
			"# short comment",
			"a = 1; # a trailing comment so long that it does not fit on the statement line",
		};
		for (const auto& sample: samples) {
			const auto once = fmt(sample, config);
			ASSERT_EQUAL_PRINT(once, fmt(once, config));
		}
	}

public:
	~FormatterTest() override = default;
};

TESTER_COMMON_MAIN("/src/formatter/tests/");
