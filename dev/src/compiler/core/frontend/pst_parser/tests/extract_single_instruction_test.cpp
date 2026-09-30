#include <frontend/pst_parser/pst.hpp>
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
		auto pst = pst::PST<>::fromContents(code, pst::PSTType::Program);

		base::Optional<pst::AccessLocked<pst::Stmt>> result;
		query::utils::withContextDo([&](query::Context& ctx) {
			result = pst::extractSingleInstruction(ctx, pst.getRootElement());
		});
		return result;
	}

	void testEmptyInput() { ASSERT_NO_VALUE(extract(""), "Expected empty for empty input"); }

	void testSingleIfInstruction() {
		ASSERT_HAS_VALUE(extract("if (a) {}"), "Simple if should be an instruction");
		ASSERT_HAS_VALUE(extract("if (a == b) { c = d; }"), "If with body should be an instruction");
		ASSERT_HAS_VALUE(
			extract("if (a == b) { c = d; } else { c = e; }"), "If-else should be an instruction"
		);
		ASSERT_HAS_VALUE(
			extract("if (a == b) c = d; else c = e;"),
			"If-else without blocks should be an instruction"
		);
		ASSERT_HAS_VALUE(extract("if AAA (x + y < 6) {}"), "Named if should be an instruction");
		ASSERT_HAS_VALUE(
			extract("if AAA (a == b) { c = d; } else { c = e; }"),
			"Named if-else should be an instruction"
		);
	}

	void testSingleWhileInstruction() {
		ASSERT_HAS_VALUE(extract("while (a) {}"), "Simple while should be an instruction");
		ASSERT_HAS_VALUE(
			extract("while (x < 5) {}"), "While with condition should be an instruction"
		);
		ASSERT_HAS_VALUE(
			extract("while (a) { foo(); }"), "While with body should be an instruction"
		);
		ASSERT_HAS_VALUE(
			extract("while AAA (x + y < 6) {}"), "Named while should be an instruction"
		);
		ASSERT_HAS_VALUE(
			extract("while MyLoop (a) { foo(); }"), "Named while with body should be an instruction"
		);
	}

	void testSingleForInstruction() {
		ASSERT_HAS_VALUE(extract("for (a: T in x) {}"), "Simple for should be an instruction");
		ASSERT_HAS_VALUE(
			extract("for (a: T in x) { foo(); }"), "For with body should be an instruction"
		);
	}

	void testSingleBlockInstruction() {
		ASSERT_HAS_VALUE(extract("block {}"), "Anonymous block should be an instruction");
		ASSERT_HAS_VALUE(extract("block B {}"), "Named block should be an instruction");
		ASSERT_HAS_VALUE(
			extract("block B { a++; }"), "Named block with body should be an instruction"
		);
	}

	void testSingleNonInstruction() {
		// Expression statements are not instructions
		ASSERT_NO_VALUE(extract("1 + 5;"), "Expected empty for arithmetic expression");
		ASSERT_NO_VALUE(extract("foo();"), "Expected empty for function call");
		ASSERT_NO_VALUE(extract("x = 42;"), "Expected empty for assignment");

		// Declarations/definitions are not instructions
		ASSERT_NO_VALUE(extract("var x: i32 = 5;"), "Expected empty for var declaration");
		ASSERT_NO_VALUE(extract("const X: W = 5;"), "Expected empty for const declaration");
		ASSERT_NO_VALUE(extract("fun foo() = {}"), "Expected empty for function declaration");
		ASSERT_NO_VALUE(extract("class Foo {}"), "Expected empty for class declaration");
		ASSERT_NO_VALUE(extract("namespace Foo {}"), "Expected empty for namespace declaration");
		ASSERT_NO_VALUE(extract("import X as x;"), "Expected empty for import statement");
		ASSERT_NO_VALUE(extract("using X;"), "Expected empty for using statement");
		ASSERT_NO_VALUE(extract("using X as Y;"), "Expected empty for `using ... as` declaration");

		// Actions (return, break, continue, throw, etc.) are not instructions
		ASSERT_NO_VALUE(extract("return 2;"), "Expected empty for return action");
		ASSERT_NO_VALUE(extract("break;"), "Expected empty for break action");
		ASSERT_NO_VALUE(extract("throw 123;"), "Expected empty for throw action");
		ASSERT_NO_VALUE(extract("redo;"), "Expected empty for redo action");
	}

	void testMultipleStatements() {
		ASSERT_NO_VALUE(extract("if (a) {}; while (a) {}"), "Expected empty for two instructions");
		ASSERT_NO_VALUE(
			extract("if (a) {}; foo();"), "Expected empty for instruction followed by expression"
		);
		ASSERT_NO_VALUE(
			extract("foo(); if (a) {}"), "Expected empty for expression followed by instruction"
		);
		ASSERT_NO_VALUE(
			extract("if (a) {}; fun bar() = {}"),
			"Expected empty for instruction followed by definition"
		);
	}

public:
	~ExtractSingleInstructionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
