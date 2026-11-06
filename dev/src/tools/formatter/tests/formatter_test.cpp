// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
#include <initializer_list>
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
		token_source->tokenize({ .keep_comments = true });
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
		token_source->tokenize({ .keep_comments = true });
		TokenSignature signature;
		collectSignature(token_source->getTokenData().tokens, signature);
		return signature;
	}

	std::string readFile(const std::string& path) {
		return std::string(fs::File(path).getContent().view().stringView());
	}

	/** One golden case: the source to format and the output it must produce. */
	using Case = std::pair<std::string_view, std::string_view>;
}

class FormatterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FormatterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// Golden output tests.
		TESTER_ADD_TEST(testEmpty);
		TESTER_ADD_TEST(testSpacing);
		TESTER_ADD_TEST(testUnaryOperators);
		TESTER_ADD_TEST(testDeclarations);
		TESTER_ADD_TEST(testBlocks);
		TESTER_ADD_TEST(testTrailingNewline);

		// Configuration.
		TESTER_ADD_TEST(testConfigAffectsOutput);
		TESTER_ADD_TEST(testConfigFromJson);
		TESTER_ADD_TEST(testInvalidIndentStyleRejected);

		// Property tests.
		TESTER_ADD_TEST(testRoundTripTokens);
		TESTER_ADD_TEST(testOperatorAdjacency);
		TESTER_ADD_TEST(testRepoSnippets);
		TESTER_ADD_TEST(testWrapKeepsTokenStream);

		// Line wrapping.
		TESTER_ADD_TEST(testWrapBracketGroups);
		TESTER_ADD_TEST(testWrapExpressions);

		// Comments.
		TESTER_ADD_TEST(testCommentPlacement);
		TESTER_ADD_TEST(testCommentWrapping);
		TESTER_ADD_TEST(testCommentsInGroups);
		TESTER_ADD_TEST(testMultiLineBlockComment);

		// Empty lines.
		TESTER_ADD_TEST(testEmptyLines);

		// Robustness.
		TESTER_ADD_TEST(testUnicodeColumnsCountCodePoints);
		TESTER_ADD_TEST(testNestingTooDeep);
	}

private:
	/**
	 * Asserts `fmt(input) == expected`, printing both on failure, and that the output is a
	 * fixed point: formatting it again changes nothing. Every golden therefore doubles as an
	 * idempotence test.
	 */
	void check(
		std::string_view    input,
		std::string_view    expected,
		const FormatConfig& config = FormatConfig::defaults()
	) {
		const auto once = fmt(input, config);
		ASSERT_EQUAL_PRINT(std::string(expected), once);
		assertTrue(
			once == fmt(once, config), base::strConcat("formatting is not idempotent for: ", input)
		);
	}

	/** Runs check() over a table of cases sharing one configuration. */
	void checkAll(
		std::initializer_list<Case> cases, const FormatConfig& config = FormatConfig::defaults()
	) {
		for (const auto& [input, expected]: cases) check(input, expected, config);
	}

	/** Asserts formatting @p source leaves its significant token stream untouched. */
	void assertTokensKept(
		std::string_view source, const FormatConfig& config = FormatConfig::defaults()
	) {
		assertTrue(
			signatureOf(source) == signatureOf(fmt(source, config)),
			base::strConcat("formatting changed the token stream for: ", source)
		);
	}

	static FormatConfig narrowConfig(u32 width) {
		FormatConfig config;
		config.max_line_length = width;
		return config;
	}

	void testEmpty() {
		ASSERT_EQUAL_PRINT(std::string(""), fmt(""));
		ASSERT_EQUAL_PRINT(std::string(""), fmt("   \n\t  \n"));
	}

	/** Normalization of inter-token spacing, the formatter's most basic job. */
	void testSpacing() {
		checkAll({
			{ "a=1;", "a = 1;\n" },
			{ "a   =   1 ;", "a = 1;\n" },
			{ "a=b+c*d;", "a = b + c * d;\n" },
			{ "x=a<b;", "x = a < b;\n" },
			{ "x=a==b;", "x = a == b;\n" },
			// Member access binds tightly on both sides.
			{ "a.b.c;", "a.b.c;\n" },
			{ "std . math . sqrt ;", "std.math.sqrt;\n" },
			{ "a .? b;", "a.?b;\n" },
			// Type annotations: no space before the colon, one after.
			{ "x:i32;", "x: i32;\n" },
			{ "y : i64 = 5 ;", "y: i64 = 5;\n" },
			// Call and index groups bind to the value they follow.
			{ "foo (a ,b );", "foo(a, b);\n" },
			{ "foo();", "foo();\n" },
			{ "arr [ i ] ;", "arr[i];\n" },
			// The attribute sigil binds to its name, and the attribute to its statement.
			{ "@Attr\nx=1;", "@Attr x = 1;\n" },
			// A comma-separated curly with no statement keyword is an inline literal.
			{ "x={1,2,3};", "x = {1, 2, 3};\n" },
			{ "block{}", "block {}\n" },
			{ "fun f()={}", "fun f() = {}\n" },
		});
	}

	void testUnaryOperators() {
		checkAll({
			// Note: `x=-1` lexes `=-` as a single operator, so a space is needed here.
			{ "x = -1;", "x = -1;\n" },
			{ "x=a- -b;", "x = a - -b;\n" },
			{ "x=(-a);", "x = (-a);\n" },
			{ "return -1;", "return -1;\n" },
			{ "t++;", "t++;\n" },
			{ "--t;", "--t;\n" },
			{ "arr[i]++;", "arr[i]++;\n" },
			{ "x = &y;", "x = &y;\n" },
			{ "f(&a, b & c);", "f(&a, b & c);\n" },
			{ "pattern T(b: ref B) -> ref D = &b.data;",
		      "pattern T(b: ref B) -> ref D = &b.data;\n" },
			// `?` is a prefix type operator: `?i32` is an optional i32.
			{ "pattern S(r: ref R) -> ?i32 = none;", "pattern S(r: ref R) -> ?i32 = none;\n" },
		});
	}

	/** Declaration syntax: imports, types, generics, custom operators, text operators. */
	void testDeclarations() {
		checkAll({
			{ "import a.b.c.d;", "import a.b.c.d;\n" },
			{ "import a.b.c as D;", "import a.b.c as D;\n" },
			{ "import a.b .* ;", "import a.b.*;\n" },
			{ "import A.B.(C, D);", "import A.B.(C, D);\n" },
			{ "import A.B .* hides D ,G, C;", "import A.B.* hides D, G, C;\n" },
			// Built-in type keywords bind tightly to their array/generic group.
			{ "var data : i32 [5] = [1, 2];", "var data: i32[5] = [1, 2];\n" },
			{ "var l : List [i32] = [];", "var l: List[i32] = [];\n" },
			// Custom types and parenthesized types do too.
			{ "var p : MyClass [3] = [a, b];", "var p: MyClass[3] = [a, b];\n" },
			{ "var g : (box Greeter) [2] = [x, y];", "var g: (box Greeter)[2] = [x, y];\n" },
			// Generic declarations use square brackets after the name.
			{ "fun grab [T] (value : T) -> Holder [T] ;", "fun grab[T](value: T) -> Holder[T];\n" },
			// Non-type keywords keep their separating space.
			{ "return [1, 2];", "return [1, 2];\n" },
			// Generic arguments bind to the `:` of the instantiation.
			{ "x = impl.max : {i32}(a, b);", "x = impl.max:{i32}(a, b);\n" },
			{ "x = pick:{char}();", "x = pick:{char}();\n" },
			// A custom operator declared as a name binds to its parameter list.
			{ "fun +* (a: i64, b: i64) -> i64 = a * b;",
		      "fun +*(a: i64, b: i64) -> i64 = a * b;\n" },
			{ "fun -*(a: i64) -> i64 = a;", "fun -*(a: i64) -> i64 = a;\n" },
			// A binary operator in an expression keeps its spaces.
			{ "x = a + (b);", "x = a + (b);\n" },
			// Text operators never glue to their operand: `notb` would be one identifier.
			{ "x = not b;", "x = not b;\n" },
			{ "x = not (b);", "x = not (b);\n" },
			{ "y = copyof x;", "y = copyof x;\n" },
			{ "z = ptrof x;", "z = ptrof x;\n" },
			{ "fun f(v: i32 | f32) -> i64 = 0i64;", "fun f(v: i32 | f32) -> i64 = 0i64;\n" },
			{ "x = ~a | b & 3 ^ 1 << 2 >> 1;", "x = ~a | b & 3 ^ 1 << 2 >> 1;\n" },
			// A `template(...)` header always takes its own line.
			{ "template(a: i64) const C: i64 = a;", "template(a: i64)\nconst C: i64 = a;\n" },
		});

		check("template(T: type) fun pick() -> i64 = {return 1;}", golden(R"(
template(T: type)
fun pick() -> i64 = {
	return 1;
}
)"));
	}

	/** Code blocks explode onto their own lines; inline literals do not. */
	void testBlocks() {
		check("fun f()={x=1;y=2;}", golden(R"(
fun f() = {
	x = 1;
	y = 2;
}
)"));
		check("fun f()={if(a){b;}}", golden(R"(
fun f() = {
	if (a) {
		b;
	}
}
)"));
		check("if(a){b;}else if(c){d;}else{e;}", golden(R"(
if (a) {
	b;
} else if (c) {
	d;
} else {
	e;
}
)"));
		check("while(a<b){c;}", golden(R"(
while (a < b) {
	c;
}
)"));
		check("class C{x:i32;fun m()={y=1;}}", golden(R"(
class C {
	x: i32;
	fun m() = {
		y = 1;
	}
}
)"));
		// Match arms carry no trailing `;`; each `case` still starts its own line.
		check("match (rect) {case Square(len) = print(a) case _ = print(b)}", golden(R"(
match (rect) {
	case Square(len) = print(a)
	case _ = print(b)
}
)"));
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
		// A curly with comma-separated expressions and no statement keyword stays a literal.
		check("let s = {1, 2, 3};", "let s = {1, 2, 3};\n");
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

	void testConfigAffectsOutput() {
		FormatConfig spaces;
		spaces.indent_style = IndentStyle::Space;
		spaces.indent_width = 2;
		check(
			"fun f()={x=1;}",
			golden(R"(
fun f() = {
  x = 1;
}
)"),
			spaces
		);

		spaces.indent_width = 4;
		check(
			"fun f()={x=1;}",
			golden(R"(
fun f() = {
    x = 1;
}
)"),
			spaces
		);

		FormatConfig tight;
		tight.space_around_operators = false;
		check("a = b + c;", "a=b+c;\n", tight);

		FormatConfig no_blanks;
		no_blanks.max_empty_lines = 0;
		check("a=1;\n\n\nb=2;", "a = 1;\nb = 2;\n", no_blanks);

		no_blanks.max_empty_lines = 1;
		check("a=1;\n\n\nb=2;", "a = 1;\n\nb = 2;\n", no_blanks);
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

		const auto defaults = FormatConfig::defaults();
		ASSERT_EQUAL(IndentStyle::Tab == defaults.indent_style, true);
		ASSERT_EQUAL(2u, defaults.max_empty_lines);
		ASSERT_EQUAL(true, defaults.space_around_operators);

		// Unknown keys are ignored, and untouched keys keep their defaults.
		const auto partial
			= FormatConfig::fromJson(nlohmann::json::parse(R"({"unknownKey": 42, "indentWidth": 8})"
		    ));
		ASSERT_EQUAL(8u, partial.indent_width);
		ASSERT_EQUAL(IndentStyle::Tab == partial.indent_style, true);
	}

	/** A misspelled or mistyped indentStyle must be rejected, not silently defaulted to tabs. */
	void testInvalidIndentStyleRejected() {
		assertThrows<nlohmann::json::exception>(
			[] {
				return FormatConfig::fromJson(nlohmann::json::parse(R"({"indentStyle": "tabs"})"));
			},
			"a misspelled indentStyle must be rejected, not silently defaulted"
		);
		assertThrows<nlohmann::json::exception>(
			[] { return FormatConfig::fromJson(nlohmann::json::parse(R"({"indentStyle": 7})")); },
			"a non-string indentStyle must be rejected"
		);
	}

	/** Formatting must never change the significant token stream of a program, comments included. */
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
			// Comments survive too, wherever they sit.
			"a=1;# trailing\nb=2;",
			"# leading\nx=1;",
			"fun f()={# inner\nx=1;}",
			"x = #{ inline block comment #} 1;",
		};
		for (const auto& sample: samples) {
			assertTokensKept(sample);
			const auto once = fmt(sample);
			ASSERT_EQUAL_PRINT(once, fmt(once));
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
		for (const auto& sample: samples) assertTokensKept(sample);

		// Even with spaces disabled, operators that would merge must stay separated.
		FormatConfig tight;
		tight.space_around_operators = false;
		for (const auto& sample: { "a - -b;", "a ** b;", "a + +b;" })
			assertTokensKept(sample, tight);
	}

	/** Real Duckling snippets copied from across the repository must round-trip and be stable. */
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
			assertTokensKept(source);
			const auto once = fmt(source);
			ASSERT_EQUAL_PRINT(once, fmt(once));
		}
	}

	/** Every wrapped form must keep the token stream intact. */
	void testWrapKeepsTokenStream() {
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
			assertTokensKept(sample, config);
			const auto once = fmt(sample, config);
			ASSERT_EQUAL_PRINT(once, fmt(once, config));
		}
	}

	/** An over-long bracket group explodes one comma-separated element per line. */
	void testWrapBracketGroups() {
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

		// Comfortably under the default 100-column limit: stays on one line.
		check("foo(a, b, c);", "foo(a, b, c);\n");
		// No top-level comma to break on, so an over-long group stays inline.
		check("foo(reallyLongSingleArgument);", "foo(reallyLongSingleArgument);\n", narrowConfig(5));
	}

	/** Expressions with no breakable group wrap at chain dots or binary operators. */
	void testWrapExpressions() {
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

		// Nested groups explode outside-in, each one level deeper.
		const std::string_view nested = "x = f(g(h(aaaa, bbbb), cccc), dddd);";
		check(
			nested,
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
			narrowConfig(20)
		);
		assertTokensKept(nested, narrowConfig(20));
	}

	/** Comments stay on the line the author put them on. */
	void testCommentPlacement() {
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
		// A comment trailing a closing brace stays with its statement.
		check("fun f() = {\n\tx = 1;\n} # done\n", golden(R"(
fun f() = {
	x = 1;
} # done
)"));
		// A `#{ ... #}` comment is reproduced verbatim and does not end the statement.
		check("x = 1; #{ c #}\n", "x = 1; #{ c #}\n");
		check("x =   #{ inline note #}   1;", "x = #{ inline note #} 1;\n");
	}

	/** An over-long line comment re-flows at word boundaries, repeating its prefix. */
	void testCommentWrapping() {
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

		// Continuation lines repeat the full multi-`#` prefix, whatever its length.
		check(
			"## returns the sum of all elements in the given list",
			golden(R"(
## returns the sum of all
## elements in the given list
)"),
			narrowConfig(30)
		);

		check(
			"### three hash comment that is long enough to need reflowing",
			golden(R"(
### three hash comment that is
### long enough to need
### reflowing
)"),
			narrowConfig(30)
		);

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

		// A fitting comment is reproduced verbatim, inner spacing included.
		check("#  keep   inner   spacing", "#  keep   inner   spacing\n");
	}

	/** A line comment inside a bracket group forces it to explode and keeps its element. */
	void testCommentsInGroups() {
		const std::string_view trailing = "foo(aaaa, # first\nbbbb, cccc);";
		check(
			trailing,
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
		assertTokensKept(trailing, narrowConfig(20));

		// A comment on its own source line keeps its own line inside the exploded group.
		check("foo(aaaa,\n# pick wisely\nbbbb);", golden(R"(
foo(
	aaaa,
	# pick wisely
	bbbb
);
)"));

		// The group fits the default line length, but a line comment can never render inline.
		check("foo(a, # note\nb);", golden(R"(
foo(
	a, # note
	b
);
)"));

		// A comment closing a group element must not leave a line holding only indentation.
		const std::string_view commaless = "foo(aaaa # why\n);";
		check(commaless, golden(R"(
foo(
	aaaa # why
);
)"));
		assertTokensKept(commaless);

		// A `#{ ... #}` comment does not end its line, so the group stays inline.
		check("foo(a, #{ ok #} b);", "foo(a, #{ ok #} b);\n");
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

	void testEmptyLines() {
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
		// More than max_empty_lines (default 2) consecutive empty lines collapse to the limit.
		check("a=1;\n\n\n\n\n\nb=2;", golden(R"(
a = 1;


b = 2;
)"));
		// Leading and trailing empty lines are always dropped.
		check("\n\n\na=1;\n\n\n", "a = 1;\n");
		check("a=1; # note\n\nb=2;", golden(R"(
a = 1; # note

b = 2;
)"));
	}

	/** Columns count characters, not bytes, so non-ASCII text wraps where an editor shows it. */
	void testUnicodeColumnsCountCodePoints() {
		// 36 visual columns, but 60 bytes.
		check(
			"foo(ἄλφαἄλφα, "
			"βῆταβῆτα, "
			"γάμμαγάμμα);",
			"foo(ἄλφαἄλφα, "
			"βῆταβῆτα, "
			"γάμμαγάμμα);\n",
			narrowConfig(40)
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

public:
	~FormatterTest() override = default;
};

TESTER_COMMON_MAIN("/src/tools/formatter/tests/");
