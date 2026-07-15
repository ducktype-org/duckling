#include <frontend/pst_parser/parsed_pst.hpp>
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
		auto pst = pst::ParsedPST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::ExprStmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleExpression(ctx, pst->getRootElement());
		});
		return result;
	}

	void testEmptyInput() {
		assertFalse(extract("").has_value(), "Expected empty for empty input");
	}

	void testSingleExpressionStatement() {
		assertTrue(extract("1 + 5;").has_value(), "Arithmetic expression should be an ExprStmt");
		assertTrue(extract("foo();").has_value(), "Function call should be an ExprStmt");
		assertTrue(extract("x = 42;").has_value(), "Assignment should be an ExprStmt");
		assertTrue(extract("obj.method(arg);").has_value(), "Method call should be an ExprStmt");
	}

	void testSingleNonExpressionStatement() {
		assertFalse(extract("const X: W = 5;").has_value(), "Expected empty for const declaration");
		assertFalse(extract("var x: i32 = 5;").has_value(), "Expected empty for var declaration");
		assertFalse(
			extract("fun foo() = {}").has_value(), "Expected empty for function declaration"
		);
		assertFalse(extract("class Foo {}").has_value(), "Expected empty for class declaration");
		assertFalse(
			extract("namespace Foo {}").has_value(), "Expected empty for namespace declaration"
		);
		assertFalse(extract("import X as x;").has_value(), "Expected empty for import statement");
		assertFalse(extract("using X;").has_value(), "Expected empty for using statement");
		assertFalse(extract("alias X = X;").has_value(), "Expected empty for alias declaration");
		assertFalse(extract("while (a) {}").has_value(), "Expected empty for while statement");
		assertFalse(extract("throw 123;").has_value(), "Expected empty for throw statement");
	}

	void testMultipleStatements() {
		assertFalse(
			extract("foo(); bar();").has_value(), "Expected empty for two expression statements"
		);
		assertFalse(
			extract("1 + 5; var x: i32 = 5;").has_value(),
			"Expected empty for expression followed by definition"
		);
		assertFalse(
			extract("var x: i32 = 5; 1 + 5;").has_value(),
			"Expected empty for definition followed by expression"
		);
		assertFalse(
			extract("fun foo() = {} fun bar() = {}").has_value(),
			"Expected empty for two function definitions"
		);
	}

public:
	~ExtractSingleExpressionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
