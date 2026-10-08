// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/parsed_pst.hpp>
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
		TESTER_ADD_TEST(testSelectorDeclarationKind);
	}

private:
	base::Optional<pst::AccessLocked<pst::Stmt>> extract(std::string_view code) {
		auto pst = pst::ParsedPST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::Stmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleDefinition(ctx, pst->getRootElement());
		});
		return result;
	}

	/**
	 * @brief Parses a single `using`/`import` and checks its `DeclKind` and declared name.
	 */
	void checkSelectorDecl(
		std::string_view code, pst::DeclKind expected_kind, std::string_view expected_name = ""
	) {
		auto pst = pst::ParsedPST<>::fromContents(code, pst::PSTType::Program);
		assertFalse(pst->hasErrors(), base::strConcat("Unexpected parse error in: ", code));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt_opt = pst::extractSingleStatement(ctx, pst->getRootElement());
			ASSERT_HAS_VALUE(stmt_opt, base::strConcat("Expected one statement in: ", code));
			auto stmt = stmt_opt.value().unlock(ctx);

			assertTrue(
				stmt->isDeclaration() == expected_kind,
				base::strConcat("Unexpected declaration kind of: ", code)
			);

			auto name = stmt->getDeclSymbolIdentifier();
			if (expected_name.empty()) {
				ASSERT_NO_VALUE(name, base::strConcat("Expected no declared name in: ", code));
			} else {
				ASSERT_HAS_VALUE(name, base::strConcat("Expected a declared name in: ", code));
				ASSERT_EQUAL(std::string(expected_name), name.value().unlock(ctx)->unwrap().str());
			}
		});
	}

	void testSelectorDeclarationKind() {
		using pst::DeclKind;
		// One bound name: indexed by that name.
		checkSelectorDecl("using a.b;", DeclKind::Symbol, "b");
		checkSelectorDecl("using a.b as c;", DeclKind::Symbol, "c");
		checkSelectorDecl("using a as c;", DeclKind::Symbol, "c");
		checkSelectorDecl("import a.b;", DeclKind::Symbol, "b");
		checkSelectorDecl("import a.b as c;", DeclKind::Symbol, "c");
		// Anything else is transparent and declares no single name.
		checkSelectorDecl("using a.b.*;", DeclKind::Transparent);
		checkSelectorDecl("using a.* hides {x, y};", DeclKind::Transparent);
		checkSelectorDecl("using a.{b};", DeclKind::Transparent);
		checkSelectorDecl("using a.b.{c as d, e};", DeclKind::Transparent);
		checkSelectorDecl("using a.a, b.b;", DeclKind::Transparent);
		checkSelectorDecl("import a.b.*;", DeclKind::Transparent);
		checkSelectorDecl("import a.b.{c, d};", DeclKind::Transparent);
		checkSelectorDecl("import a.a, b.b;", DeclKind::Transparent);
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
		ASSERT_HAS_VALUE(extract("using x as y;"), "Simple alias to symbol should be returned");
		ASSERT_HAS_VALUE(
			extract("using obj.member as foo;"), "Alias to dotted name should be returned"
		);
		ASSERT_HAS_VALUE(
			extract("using outer.inner.core.foo as nested;"),
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
