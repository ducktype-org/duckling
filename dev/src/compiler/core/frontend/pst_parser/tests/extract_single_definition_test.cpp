#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/utility.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

class ExtractSingleDefinitionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExtractSingleDefinitionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleVariableDefinition);
		TESTER_ADD_TEST(testSingleConstDefinition);
		TESTER_ADD_TEST(testSingleFunctionDefinition);
		TESTER_ADD_TEST(testSingleClassDefinition);
		TESTER_ADD_TEST(testSingleNamespaceDefinition);
		TESTER_ADD_TEST(testSingleUsingStatement);
		TESTER_ADD_TEST(testSingleAliasDefinition);
		TESTER_ADD_TEST(testMultipleStatements);
	}

private:
	base::Optional<pst::AccessLocked<pst::Stmt>> extract(std::string_view code) {
		auto pst = pst::PST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::Stmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleDefinition(ctx, pst.getRootElement());
		});
		return result;
	}

	void testEmptyInput() {
		assertFalse(extract("").has_value(), "Expected empty for empty input");
	}

	void testSingleVariableDefinition() {
		assertTrue(
			extract("var x: i32 = 5;").has_value(), "Variable declaration should be returned"
		);
		assertTrue(
			extract("var foo: String = \"hello\";").has_value(),
			"Variable with string should be returned"
		);
		assertTrue(
			extract("var data: Array<i32> = [];").has_value(),
			"Variable with complex type should be returned"
		);
	}

	void testSingleConstDefinition() {
		assertTrue(extract("const X: W = 5;").has_value(), "Const declaration should be returned");
		assertTrue(
			extract("const MAX: i32 = 100;").has_value(), "Const with value should be returned"
		);
	}

	void testSingleFunctionDefinition() {
		assertTrue(extract("fun foo() = {}").has_value(), "Function declaration should be returned");
		assertTrue(
			extract("fun bar(x: i32): i32 = { x + 1 }").has_value(),
			"Function with parameters and body should be returned"
		);
		assertTrue(
			extract("fun baz(a: i32, b: String) = {}").has_value(),
			"Function with multiple parameters should be returned"
		);
	}

	void testSingleClassDefinition() {
		assertTrue(extract("class Foo {}").has_value(), "Class declaration should be returned");
		assertTrue(
			extract("class Bar { var x: i32; }").has_value(), "Class with members should be returned"
		);
		assertTrue(
			extract("class Baz<T> { fun method() = {} }").has_value(),
			"Generic class should be returned"
		);
	}

	void testSingleNamespaceDefinition() {
		assertTrue(
			extract("namespace Foo {}").has_value(), "Namespace declaration should be returned"
		);
		assertTrue(
			extract("namespace Bar { var x: i32 = 1; }").has_value(),
			"Namespace with content should be returned"
		);
	}

	void testSingleUsingStatement() {
		assertTrue(extract("using X;").has_value(), "Using statement should be returned");
		assertTrue(
			extract("using MyNamespace;").has_value(), "Using with namespace should be returned"
		);
	}

	void testSingleAliasDefinition() {
		assertTrue(extract("alias y = x;").has_value(), "Simple alias to symbol should be returned");
		assertTrue(
			extract("alias foo = obj.member;").has_value(), "Alias to dotted name should be returned"
		);
		assertTrue(
			extract("alias nested = outer.inner.core.foo;").has_value(),
			"Alias to nested dotted name should be returned"
		);
	}

	void testMultipleStatements() {
		assertFalse(
			extract("var x: i32 = 5; var y: i32 = 10;").has_value(),
			"Expected empty for two variable declarations"
		);
		assertFalse(
			extract("fun foo() = {}; fun bar() = {}").has_value(),
			"Expected empty for two function declarations"
		);
		assertFalse(
			extract("class Foo {}; class Bar {}").has_value(),
			"Expected empty for two class declarations"
		);
		assertFalse(
			extract("var x: i32 = 5; fun foo() = {}").has_value(),
			"Expected empty for variable and function"
		);
		assertFalse(
			extract("const X: i32 = 1; using Y;").has_value(), "Expected empty for const and using"
		);
	}

public:
	~ExtractSingleDefinitionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
