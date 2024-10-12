#include <tester/tester.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <lang_definitions/operator_precedence.hpp>

class SimpleLangDefTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleLangDefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		lang_def::key_spec_op::init();
		lang_def::operator_precedence::init();

		TESTER_ADD_TEST(simpleOperatorPrecedenceTest);
		TESTER_ADD_TEST(simpleOperatorAssociativityTest);
		TESTER_ADD_TEST(exportsForLSPTest);
	}

private:
	void simpleOperatorPrecedenceTest() {
		using namespace lang_def;

		auto period   = operatorPrecedence(base::StrID("."), OperatorType::Binary);
		auto period_2 = operatorPrecedence(Operator::Period, OperatorType::Binary);

		auto inc = operatorPrecedence(Operator::DoublePlus, OperatorType::UnaryRight);
		auto neg = operatorPrecedence(Operator::Minus, OperatorType::UnaryRight);

		auto multiply = operatorPrecedence(Operator::Multiply, OperatorType::Binary);
		auto divide   = operatorPrecedence(Operator::Divide, OperatorType::Binary);

		auto add      = operatorPrecedence(Operator::Plus, OperatorType::Binary);
		auto subtract = operatorPrecedence(Operator::Minus, OperatorType::Binary);

		auto assign = operatorPrecedence(Operator::Assign, OperatorType::Binary);

		assertTrue(period == period_2, "Same operators have different precedence");
		assertTrue(multiply == divide, "*, / have different precedence");
		assertTrue(add == subtract, "+, - have different precedence");

		assertTrue(period < add, ". is not the first operator");
		assertTrue(period < inc, ". is not the first operator");
		assertTrue(period < neg, ". is not the first operator");
		assertTrue(period < subtract, ". is not the first operator");
		assertTrue(period < multiply, ". is not the first operator");
		assertTrue(period < divide, ". is not the first operator");
		assertTrue(period < assign, ". is not the first operator");

		assertTrue(multiply < subtract, "* is not before -");
	}

	void simpleOperatorAssociativityTest() {
		using namespace lang_def;

		auto period   = operatorAssociativity(base::StrID("."), OperatorType::Binary);
		auto period_2 = operatorAssociativity(Operator::Period, OperatorType::Binary);

		auto inc = operatorAssociativity(Operator::DoublePlus, OperatorType::UnaryRight);

		[[maybe_unused]] auto neg
			= operatorAssociativity(Operator::Minus, OperatorType::UnaryRight);

		auto multiply = operatorAssociativity(Operator::Multiply, OperatorType::Binary);

		[[maybe_unused]] auto divide
			= operatorAssociativity(Operator::Divide, OperatorType::Binary);

		auto add = operatorAssociativity(Operator::Plus, OperatorType::Binary);

		[[maybe_unused]] auto subtract
			= operatorAssociativity(Operator::Minus, OperatorType::Binary);

		[[maybe_unused]] auto assign
			= operatorAssociativity(Operator::Assign, OperatorType::Binary);

		assertTrue(period == period_2, "Same operators have different associativity");

		assertTrue(multiply == OperatorAssociativity::LeftToRight, "* has bad associativity");
		assertTrue(add == OperatorAssociativity::LeftToRight, "* has bad associativity");

		assertTrue(inc == OperatorAssociativity::RightToLeft, "++ has bad associativity");
	}

	void exportsForLSPTest() {
		ASSERT_EQUAL(lang_def::getKeywords().size(), 66);
		ASSERT_EQUAL(lang_def::getSpecials().size(), 6);
		ASSERT_EQUAL(lang_def::getOperators().size(), 15);
	}
};

TESTER_COMMON_MAIN("/common/lang_definitions/tests/");
