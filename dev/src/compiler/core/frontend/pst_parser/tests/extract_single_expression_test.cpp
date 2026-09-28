#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/utility.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

class ExtractSingleExpressionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExtractSingleExpressionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleExpressionStatement);
		TESTER_ADD_TEST(testSingleNonExpressionStatement);
		TESTER_ADD_TEST(testMultipleStatements);
	}

private:
	base::Optional<pst::AccessLocked<pst::ExprStmt>> extract(std::string_view code) {
		auto pst = pst::PST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::ExprStmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleExpression(ctx, pst.getRootElement());
		});
		return result;
	}

	void testEmptyInput() { ASSERT_NO_VALUE(extract(""), "Expected empty for empty input"); }

	void testSingleExpressionStatement() {
		ASSERT_HAS_VALUE(extract("1 + 5;"), "Arithmetic expression should be an ExprStmt");
		ASSERT_HAS_VALUE(extract("foo();"), "Function call should be an ExprStmt");
		ASSERT_HAS_VALUE(extract("x = 42;"), "Assignment should be an ExprStmt");
		ASSERT_HAS_VALUE(extract("obj.method(arg);"), "Method call should be an ExprStmt");
	}

	void testSingleNonExpressionStatement() {
		ASSERT_NO_VALUE(extract("const X: W = 5;"), "Expected empty for const declaration");
		ASSERT_NO_VALUE(extract("var x: i32 = 5;"), "Expected empty for var declaration");
		ASSERT_NO_VALUE(extract("fun foo() = {}"), "Expected empty for function declaration");
		ASSERT_NO_VALUE(extract("class Foo {}"), "Expected empty for class declaration");
		ASSERT_NO_VALUE(extract("namespace Foo {}"), "Expected empty for namespace declaration");
		ASSERT_NO_VALUE(extract("import X as x;"), "Expected empty for import statement");
		ASSERT_NO_VALUE(extract("using X;"), "Expected empty for using statement");
		ASSERT_NO_VALUE(extract("using X as Y;"), "Expected empty for `using ... as` declaration");
		ASSERT_NO_VALUE(extract("while (a) {}"), "Expected empty for while statement");
		ASSERT_NO_VALUE(extract("throw 123;"), "Expected empty for throw statement");
	}

	void testMultipleStatements() {
		ASSERT_NO_VALUE(extract("foo(); bar();"), "Expected empty for two expression statements");
		ASSERT_NO_VALUE(
			extract("1 + 5; var x: i32 = 5;"), "Expected empty for expression followed by definition"
		);
		ASSERT_NO_VALUE(
			extract("var x: i32 = 5; 1 + 5;"), "Expected empty for definition followed by expression"
		);
		ASSERT_NO_VALUE(
			extract("fun foo() = {} fun bar() = {}"), "Expected empty for two function definitions"
		);
	}

public:
	~ExtractSingleExpressionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
