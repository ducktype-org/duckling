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

	void testEmptyInput() { ASSERT_NO_VALUE(extract(""), "Expected empty for empty input"); }

	void testSingleVariableDefinition() {
		ASSERT_HAS_VALUE(extract("var x: i32 = 5;"), "Variable declaration should be returned");
		ASSERT_HAS_VALUE(
			extract("var foo: String = \"hello\";"), "Variable with string should be returned"
		);
		ASSERT_HAS_VALUE(
			extract("var data: Array<i32> = [];"), "Variable with complex type should be returned"
		);
	}

	void testSingleConstDefinition() {
		ASSERT_HAS_VALUE(extract("const X: W = 5;"), "Const declaration should be returned");
		ASSERT_HAS_VALUE(extract("const MAX: i32 = 100;"), "Const with value should be returned");
	}

	void testSingleFunctionDefinition() {
		ASSERT_HAS_VALUE(extract("fun foo() = {}"), "Function declaration should be returned");
		ASSERT_HAS_VALUE(
			extract("fun bar(x: i32): i32 = { x + 1 }"),
			"Function with parameters and body should be returned"
		);
		ASSERT_HAS_VALUE(
			extract("fun baz(a: i32, b: String) = {}"),
			"Function with multiple parameters should be returned"
		);
	}

	void testSingleClassDefinition() {
		ASSERT_HAS_VALUE(extract("class Foo {}"), "Class declaration should be returned");
		ASSERT_HAS_VALUE(
			extract("class Bar { var x: i32; }"), "Class with members should be returned"
		);
		ASSERT_HAS_VALUE(
			extract("class Baz<T> { fun method() = {} }"), "Generic class should be returned"
		);
	}

	void testSingleNamespaceDefinition() {
		ASSERT_HAS_VALUE(extract("namespace Foo {}"), "Namespace declaration should be returned");
		ASSERT_HAS_VALUE(
			extract("namespace Bar { var x: i32 = 1; }"), "Namespace with content should be returned"
		);
	}

	void testSingleUsingStatement() {
		ASSERT_HAS_VALUE(extract("using X;"), "Using statement should be returned");
		ASSERT_HAS_VALUE(extract("using MyNamespace;"), "Using with namespace should be returned");
	}

	void testSingleAliasDefinition() {
		ASSERT_HAS_VALUE(extract("alias y = x;"), "Simple alias to symbol should be returned");
		ASSERT_HAS_VALUE(
			extract("alias foo = obj.member;"), "Alias to dotted name should be returned"
		);
		ASSERT_HAS_VALUE(
			extract("alias nested = outer.inner.core.foo;"),
			"Alias to nested dotted name should be returned"
		);
	}

	void testMultipleStatements() {
		ASSERT_NO_VALUE(
			extract("var x: i32 = 5; var y: i32 = 10;"),
			"Expected empty for two variable declarations"
		);
		ASSERT_NO_VALUE(
			extract("fun foo() = {}; fun bar() = {}"), "Expected empty for two function declarations"
		);
		ASSERT_NO_VALUE(
			extract("class Foo {}; class Bar {}"), "Expected empty for two class declarations"
		);
		ASSERT_NO_VALUE(
			extract("var x: i32 = 5; fun foo() = {}"), "Expected empty for variable and function"
		);
		ASSERT_NO_VALUE(extract("const X: i32 = 1; using Y;"), "Expected empty for const and using");
	}

public:
	~ExtractSingleDefinitionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
