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

		TESTER_ADD_TEST(testSingleArithmeticExpr);
		TESTER_ADD_TEST(testSingleFunctionCall);
		TESTER_ADD_TEST(testSingleAssignment);
		TESTER_ADD_TEST(testSingleMethodCall);

		TESTER_ADD_TEST(testSingleConstDecl);
		TESTER_ADD_TEST(testSingleVarDecl);
		TESTER_ADD_TEST(testSingleFunDecl);
		TESTER_ADD_TEST(testSingleClassDecl);
		TESTER_ADD_TEST(testSingleNamespaceDecl);
		TESTER_ADD_TEST(testSingleImport);
		TESTER_ADD_TEST(testSingleUsing);
		TESTER_ADD_TEST(testSingleAlias);
		TESTER_ADD_TEST(testSingleWhile);
		TESTER_ADD_TEST(testSingleThrow);

		TESTER_ADD_TEST(testTwoExpressionStmts);
		TESTER_ADD_TEST(testExpressionThenDefinition);
		TESTER_ADD_TEST(testDefinitionThenExpression);
		TESTER_ADD_TEST(testTwoDefinitions);
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

	void testEmptyInput() {
		assertFalse(extract("").has_value(), "Expected empty for empty input");
	}

	void testSingleArithmeticExpr() {
		assertTrue(extract("1 + 5;").has_value(), "Arithmetic expression should be an ExprStmt");
	}

	void testSingleFunctionCall() {
		assertTrue(extract("foo();").has_value(), "Function call should be an ExprStmt");
	}

	void testSingleAssignment() {
		assertTrue(extract("x = 42;").has_value(), "Assignment should be an ExprStmt");
	}

	void testSingleMethodCall() {
		assertTrue(extract("obj.method(arg);").has_value(), "Method call should be an ExprStmt");
	}

	void testSingleConstDecl() {
		assertFalse(
			extract("const X: W = 5;").has_value(), "Expected empty for a single const declaration"
		);
	}

	void testSingleVarDecl() {
		assertFalse(
			extract("var x: i32 = 5;").has_value(), "Expected empty for a single var declaration"
		);
	}

	void testSingleFunDecl() {
		assertFalse(
			extract("fun foo() = {}").has_value(), "Expected empty for a single function declaration"
		);
	}

	void testSingleClassDecl() {
		assertFalse(
			extract("class Foo {}").has_value(), "Expected empty for a single class declaration"
		);
	}

	void testSingleNamespaceDecl() {
		assertFalse(
			extract("namespace Foo {}").has_value(),
			"Expected empty for a single namespace declaration"
		);
	}

	void testSingleImport() {
		assertFalse(extract("import X as x;").has_value(), "Expected empty for an import statement");
	}

	void testSingleUsing() {
		assertFalse(extract("using X;").has_value(), "Expected empty for a using statement");
	}

	void testSingleAlias() {
		assertFalse(extract("alias X = X;").has_value(), "Expected empty for an alias declaration");
	}

	void testSingleWhile() {
		assertFalse(extract("while (a) {}").has_value(), "Expected empty for a while statement");
	}

	void testSingleThrow() {
		assertFalse(extract("throw 123;").has_value(), "Expected empty for a throw statement");
	}

	void testTwoExpressionStmts() {
		assertFalse(
			extract("foo(); bar();").has_value(),
			"Expected empty for two consecutive expression statements"
		);
	}

	void testExpressionThenDefinition() {
		assertFalse(
			extract("1 + 5; var x: i32 = 5;").has_value(),
			"Expected empty for expression followed by definition"
		);
	}

	void testDefinitionThenExpression() {
		assertFalse(
			extract("var x: i32 = 5; 1 + 5;").has_value(),
			"Expected empty for definition followed by expression"
		);
	}

	void testTwoDefinitions() {
		assertFalse(
			extract("fun foo() = {} fun bar() = {}").has_value(),
			"Expected empty for two function definitions"
		);
	}

public:
	~ExtractSingleExpressionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
