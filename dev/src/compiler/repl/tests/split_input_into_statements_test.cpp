#include <repl/utils.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

class SplitInputIntoStatementsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SplitInputIntoStatementsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleStatement);
		TESTER_ADD_TEST(testMultipleStatements);
		TESTER_ADD_TEST(testSnippets);
		TESTER_ADD_TEST(testParseError);
	}

private:
	std::string readSnippet(const std::string& local_path) {
		return std::string{ fs::File(path(local_path)).getContent().view().stringView() };
	}

	void testEmptyInput() {
		auto result = compiler::repl::splitInputIntoStatements("");
		assertTrue(result.has_value(), "Expected success for empty input");
		assertTrue(result->empty(), "Expected no statements for empty input");
	}

	void testSingleStatement() {
		auto check_single = [&](std::string_view code) {
			auto result = compiler::repl::splitInputIntoStatements(code);
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(1UL, result->size());
			ASSERT_EQUAL(std::string(code), (*result)[0]);
		};
		check_single("1 + 5;");
		check_single("var x: i32 = 5;");
		check_single("const X: i32 = 5;");
		check_single("fun foo() = {}");
		check_single("class Foo {}");
		check_single("namespace Foo {}");
	}

	void testMultipleStatements() {
		// two expression statements
		{
			auto result = compiler::repl::splitInputIntoStatements("1 + 5;\n2 * 3;");
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(2UL, result->size());
			ASSERT_EQUAL(std::string("1 + 5;"), (*result)[0]);
			ASSERT_EQUAL(std::string("2 * 3;"), (*result)[1]);
		}
		// mixed statement kinds, order preserved
		{
			auto result
				= compiler::repl::splitInputIntoStatements("var x: i32 = 1;\n1 + 5;\nfun foo() = {}"
			    );
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("var x: i32 = 1;"), (*result)[0]);
			ASSERT_EQUAL(std::string("1 + 5;"), (*result)[1]);
			ASSERT_EQUAL(std::string("fun foo() = {}"), (*result)[2]);
		}
		// source order preserved for three calls
		{
			auto result = compiler::repl::splitInputIntoStatements("foo();\nbar();\nbaz();");
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("foo();"), (*result)[0]);
			ASSERT_EQUAL(std::string("bar();"), (*result)[1]);
			ASSERT_EQUAL(std::string("baz();"), (*result)[2]);
		}
		// multi-line function body captured as a single statement
		{
			std::string code   = "fun foo() = {\n    var x: i32 = 1;\n}";
			auto        result = compiler::repl::splitInputIntoStatements(code);
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(1UL, result->size());
			ASSERT_EQUAL(code, (*result)[0]);
		}
		// multiple declaration kinds
		{
			auto result = compiler::repl::splitInputIntoStatements(
				"class Foo {}\nnamespace Bar {}\nfun baz() = {}"
			);
			assertTrue(result.has_value(), "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("class Foo {}"), (*result)[0]);
			ASSERT_EQUAL(std::string("namespace Bar {}"), (*result)[1]);
			ASSERT_EQUAL(std::string("fun baz() = {}"), (*result)[2]);
		}
	}

	void testSnippets() {
		{
			auto result = compiler::repl::splitInputIntoStatements(
				readSnippet("snippets/loops_and_declarations.duck")
			);
			assertTrue(result.has_value(), "Expected success for loops_and_declarations snippet");
			ASSERT_EQUAL(4UL, result->size());
			ASSERT_EQUAL(std::string("var counter: i32 = 0;"), (*result)[0]);
			ASSERT_EQUAL(std::string("class Storage {}"), (*result)[3]);
			assertTrue((*result)[1].starts_with("while"), "Second statement must be the while loop");
			assertTrue(
				(*result)[1].find("counter + 1") != std::string::npos,
				"While loop must contain the counter increment"
			);
			assertTrue(
				(*result)[2].starts_with("fun doubleCounter"),
				"Third statement must be the function declaration"
			);
			assertTrue(
				(*result)[2].find("while (i < n)") != std::string::npos,
				"Function body must contain nested while loop"
			);
		}
		{
			auto result = compiler::repl::splitInputIntoStatements(
				readSnippet("snippets/declarations_only.duck")
			);
			assertTrue(result.has_value(), "Expected success for declarations_only snippet");
			ASSERT_EQUAL(6UL, result->size());
			ASSERT_EQUAL(std::string("import X as x;"), (*result)[0]);
			ASSERT_EQUAL(std::string("using X;"), (*result)[1]);
			ASSERT_EQUAL(std::string("alias X = X;"), (*result)[2]);
			ASSERT_EQUAL(std::string("class Foo {}"), (*result)[3]);
			ASSERT_EQUAL(std::string("namespace Math {}"), (*result)[4]);
			assertTrue(
				(*result)[5].starts_with("fun add"),
				"Last statement must be the function declaration"
			);
			assertTrue(
				(*result)[5].find("return a + b;") != std::string::npos,
				"Function body must contain the return statement"
			);
		}
	}

	void testParseError() {
		// Unclosed brace — guaranteed parse error
		auto result = compiler::repl::splitInputIntoStatements("{");
		assertTrue(!result.has_value(), "Expected error for invalid syntax");
		assertTrue(!result.error().empty(), "Expected non-empty error message");

		auto result2 = compiler::repl::splitInputIntoStatements("fun foo() = {");
		assertTrue(!result2.has_value(), "Expected error for unclosed function body");
		assertTrue(!result2.error().empty(), "Expected non-empty error message");
	}

public:
	~SplitInputIntoStatementsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
