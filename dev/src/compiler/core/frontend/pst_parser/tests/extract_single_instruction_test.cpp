#include <frontend/pst_parser/parsed_pst.hpp>
#include <frontend/pst_parser/utility.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

class ExtractSingleInstructionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExtractSingleInstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testEmptyInput);
		TESTER_ADD_TEST(testSingleIfInstruction);
		TESTER_ADD_TEST(testSingleWhileInstruction);
		TESTER_ADD_TEST(testSingleForInstruction);
		TESTER_ADD_TEST(testSingleBlockInstruction);
		TESTER_ADD_TEST(testSingleNonInstruction);
		TESTER_ADD_TEST(testMultipleStatements);
	}

private:
	base::Optional<pst::AccessLocked<pst::Stmt>> extract(std::string_view code) {
		auto pst = pst::ParsedPST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::Stmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleInstruction(ctx, pst->getRootElement());
		});
		return result;
	}

	void testEmptyInput() {
		assertFalse(extract("").has_value(), "Expected empty for empty input");
	}

	void testSingleIfInstruction() {
		assertTrue(extract("if (a) {}").has_value(), "Simple if should be an instruction");
		assertTrue(
			extract("if (a == b) { c = d; }").has_value(), "If with body should be an instruction"
		);
		assertTrue(
			extract("if (a == b) { c = d; } else { c = e; }").has_value(),
			"If-else should be an instruction"
		);
		assertTrue(
			extract("if (a == b) c = d; else c = e;").has_value(),
			"If-else without blocks should be an instruction"
		);
		assertTrue(
			extract("if AAA (x + y < 6) {}").has_value(), "Named if should be an instruction"
		);
		assertTrue(
			extract("if AAA (a == b) { c = d; } else { c = e; }").has_value(),
			"Named if-else should be an instruction"
		);
	}

	void testSingleWhileInstruction() {
		assertTrue(extract("while (a) {}").has_value(), "Simple while should be an instruction");
		assertTrue(
			extract("while (x < 5) {}").has_value(), "While with condition should be an instruction"
		);
		assertTrue(
			extract("while (a) { foo(); }").has_value(), "While with body should be an instruction"
		);
		assertTrue(
			extract("while AAA (x + y < 6) {}").has_value(), "Named while should be an instruction"
		);
		assertTrue(
			extract("while MyLoop (a) { foo(); }").has_value(),
			"Named while with body should be an instruction"
		);
	}

	void testSingleForInstruction() {
		assertTrue(extract("for (a: T in x) {}").has_value(), "Simple for should be an instruction");
		assertTrue(
			extract("for (a: T in x) { foo(); }").has_value(),
			"For with body should be an instruction"
		);
	}

	void testSingleBlockInstruction() {
		assertTrue(extract("block {}").has_value(), "Anonymous block should be an instruction");
		assertTrue(extract("block B {}").has_value(), "Named block should be an instruction");
		assertTrue(
			extract("block B { a++; }").has_value(), "Named block with body should be an instruction"
		);
	}

	void testSingleNonInstruction() {
		// Expression statements are not instructions
		assertFalse(extract("1 + 5;").has_value(), "Expected empty for arithmetic expression");
		assertFalse(extract("foo();").has_value(), "Expected empty for function call");
		assertFalse(extract("x = 42;").has_value(), "Expected empty for assignment");

		// Declarations/definitions are not instructions
		assertFalse(extract("var x: i32 = 5;").has_value(), "Expected empty for var declaration");
		assertFalse(extract("const X: W = 5;").has_value(), "Expected empty for const declaration");
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

		// Actions (return, break, continue, throw, etc.) are not instructions
		assertFalse(extract("return 2;").has_value(), "Expected empty for return action");
		assertFalse(extract("break;").has_value(), "Expected empty for break action");
		assertFalse(extract("throw 123;").has_value(), "Expected empty for throw action");
		assertFalse(extract("redo;").has_value(), "Expected empty for redo action");
	}

	void testMultipleStatements() {
		assertFalse(
			extract("if (a) {}; while (a) {}").has_value(), "Expected empty for two instructions"
		);
		assertFalse(
			extract("if (a) {}; foo();").has_value(),
			"Expected empty for instruction followed by expression"
		);
		assertFalse(
			extract("foo(); if (a) {}").has_value(),
			"Expected empty for expression followed by instruction"
		);
		assertFalse(
			extract("if (a) {}; fun bar() = {}").has_value(),
			"Expected empty for instruction followed by definition"
		);
	}

public:
	~ExtractSingleInstructionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
