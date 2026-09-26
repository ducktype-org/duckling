#include <driver/repl_utils/repl_split_helpers.hpp>

#include <base/types/ints.hpp>

#include <tester/tester.hpp>

class SplitInputIntoStatementsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SplitInputIntoStatementsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleStatement);
		TESTER_ADD_TEST(testMultipleStatements);
		TESTER_ADD_TEST(testIdempotency);
		TESTER_ADD_TEST(testParseError);
	}

private:
	void testEmptyInput() {
		auto result = compiler::repl::splitInputIntoStatements("");
		ASSERT_HAS_VALUE(result, "Expected success for empty input");
		assertTrue(result->empty(), "Expected no statements for empty input");
	}

	void testSingleStatement() {
		auto check_single = [&](std::string_view code) {
			auto result = compiler::repl::splitInputIntoStatements(code);
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(1UL, result->size());
			ASSERT_EQUAL(std::string(code), (*result)[0]);
		};
		check_single("1 + 5;");
		check_single("var x: i32 = 5;");
		check_single("const X: i32 = 5;");
		check_single("fun foo() = {}");
		check_single("class Foo {}");
		check_single("namespace Foo {}");
		check_single("import Foo as foo;");
		check_single("using Foo;");
		check_single("using Foo as Bar;");
		check_single("using a.b.{c, d as e};");
		check_single("import a.* hides {b, c};");
		check_single("const LIMIT: i32 = 100;");
	}

	void testMultipleStatements() {
		// two expression statements
		{
			auto result = compiler::repl::splitInputIntoStatements("1 + 5;\n2 * 3;");
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(2UL, result->size());
			ASSERT_EQUAL(std::string("1 + 5;"), (*result)[0]);
			ASSERT_EQUAL(std::string("2 * 3;"), (*result)[1]);
		}
		// mixed statement kinds, order preserved
		{
			auto result
				= compiler::repl::splitInputIntoStatements("var x: i32 = 1;\n1 + 5;\nfun foo() = {}"
			    );
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("var x: i32 = 1;"), (*result)[0]);
			ASSERT_EQUAL(std::string("1 + 5;"), (*result)[1]);
			ASSERT_EQUAL(std::string("fun foo() = {}"), (*result)[2]);
		}
		// source order preserved for three calls
		{
			auto result = compiler::repl::splitInputIntoStatements("foo();\nbar();\nbaz();");
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("foo();"), (*result)[0]);
			ASSERT_EQUAL(std::string("bar();"), (*result)[1]);
			ASSERT_EQUAL(std::string("baz();"), (*result)[2]);
		}
		// multi-line function body captured as a single statement
		{
			std::string code   = "fun foo() = {\n    var x: i32 = 1;\n}";
			auto        result = compiler::repl::splitInputIntoStatements(code);
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(1UL, result->size());
			ASSERT_EQUAL(code, (*result)[0]);
		}
		// multiple declaration kinds
		{
			auto result = compiler::repl::splitInputIntoStatements(
				"class Foo {}\nnamespace Bar {}\nfun baz() = {}"
			);
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(3UL, result->size());
			ASSERT_EQUAL(std::string("class Foo {}"), (*result)[0]);
			ASSERT_EQUAL(std::string("namespace Bar {}"), (*result)[1]);
			ASSERT_EQUAL(std::string("fun baz() = {}"), (*result)[2]);
		}
		// const declaration mixed with an expression
		{
			auto result
				= compiler::repl::splitInputIntoStatements("const LIMIT: i32 = 100;\nLIMIT * 2;");
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(2UL, result->size());
			ASSERT_EQUAL(std::string("const LIMIT: i32 = 100;"), (*result)[0]);
			ASSERT_EQUAL(std::string("LIMIT * 2;"), (*result)[1]);
		}
		// multi-line class body is a single statement
		{
			std::string code   = "class Point {\n    var x: i32 = 0;\n    var y: i32 = 0;\n}";
			auto        result = compiler::repl::splitInputIntoStatements(code);
			ASSERT_HAS_VALUE(result, "Expected success");
			ASSERT_EQUAL(1UL, result->size());
			ASSERT_EQUAL(code, (*result)[0]);
		}
	}

	void testIdempotency() {
		const std::string_view code = "var x: i32 = 1;\n1 + 5;\nfun foo() = {}";

		auto first  = compiler::repl::splitInputIntoStatements(code);
		auto second = compiler::repl::splitInputIntoStatements(code);

		ASSERT_HAS_VALUE(first, "First call must succeed");
		ASSERT_HAS_VALUE(second, "Second call must succeed");
		ASSERT_EQUAL(first->size(), second->size());
		for (usize i = 0; i < first->size(); ++i) ASSERT_EQUAL((*first)[i], (*second)[i]);

		auto err1 = compiler::repl::splitInputIntoStatements("{\nfun broken() = {");
		auto err2 = compiler::repl::splitInputIntoStatements("{\nfun broken() = {");
		ASSERT_NO_VALUE(err1, "First error call must fail");
		ASSERT_NO_VALUE(err2, "Second error call must fail");
		ASSERT_EQUAL(err1.error(), err2.error());
	}

	void testParseError() {
		auto check_error = [&](std::string_view code, std::string_view label) {
			auto result = compiler::repl::splitInputIntoStatements(code);
			ASSERT_NO_VALUE(result, "Expected parse error: " + std::string(label));
			assertTrue(
				!result.error().empty(), "Error message must be non-empty: " + std::string(label)
			);
		};

		check_error("{", "lone open brace");
		check_error("fun foo() = {", "unclosed function body");
		check_error("class Foo {", "unclosed class body");
		check_error("namespace N {", "unclosed namespace body");

		check_error("fun outer() = {\n    fun inner() = {\n", "nested unclosed blocks");

		// Error recovery: a valid statement followed by a broken one
		// — the whole input must still be rejected
		check_error("var x: i32 = 1;\n{", "valid then unclosed brace");
	}

public:
	~SplitInputIntoStatementsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
