#include <lang_definitions/key_spec_op.hpp>
#include <lang_definitions/operator_precedence.hpp>
#include <tester/tester.hpp>

class SimpleLangDefTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleLangDefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleOperatorPrecedenceTest);
		TESTER_ADD_TEST(simpleOperatorAssociativityTest);
		TESTER_ADD_TEST(exportsForLSPTest);
	}

private:
	void simpleOperatorPrecedenceTest() {
		using namespace lang_def;

		auto period   = operatorPrecedence(base::StrID("."), OperatorType::Binary);
		auto period_2 = operatorPrecedence(NamedOperator::Period, OperatorType::Binary);

		auto inc = operatorPrecedence(NamedOperator::DoublePlus, OperatorType::UnaryRight);
		auto neg = operatorPrecedence(NamedOperator::Minus, OperatorType::UnaryRight);

		auto exponentiate = operatorPrecedence(NamedOperator::Exponentiate, OperatorType::Binary);

		auto multiply  = operatorPrecedence(NamedOperator::Multiply, OperatorType::Binary);
		auto divide    = operatorPrecedence(NamedOperator::Divide, OperatorType::Binary);
		auto remainder = operatorPrecedence(NamedOperator::Remainder, OperatorType::Binary);

		auto add      = operatorPrecedence(NamedOperator::Plus, OperatorType::Binary);
		auto subtract = operatorPrecedence(NamedOperator::Minus, OperatorType::Binary);

		auto assign = operatorPrecedence(NamedOperator::Assign, OperatorType::Binary);

		assertTrue(period == period_2, "Same operators have different precedence");
		assertTrue(multiply == divide, "*, / have different precedence");
		assertTrue(divide == remainder, "/, % have different precedence");
		assertTrue(add == subtract, "+, - have different precedence");

		assertTrue(period < add, ". is not the first operator");
		assertTrue(period < inc, ". is not the first operator");
		assertTrue(period < neg, ". is not the first operator");

		assertTrue(inc < exponentiate, "++ is not before **");
		assertTrue(neg < exponentiate, "-() is not before **");
		assertTrue(exponentiate < multiply, "** is not before *");
		assertTrue(multiply < add, "* is not before +");
		assertTrue(add < assign, "+ is not before =");
	}

	void simpleOperatorAssociativityTest() {
		using namespace lang_def;

		auto period   = operatorAssociativity(base::StrID("."), OperatorType::Binary);
		auto period_2 = operatorAssociativity(NamedOperator::Period, OperatorType::Binary);

		auto inc = operatorAssociativity(NamedOperator::DoublePlus, OperatorType::UnaryRight);

		[[maybe_unused]] auto neg
			= operatorAssociativity(NamedOperator::Minus, OperatorType::UnaryRight);

		auto exponentiate
			= operatorAssociativity(NamedOperator::Exponentiate, OperatorType::Binary);

		auto multiply = operatorAssociativity(NamedOperator::Multiply, OperatorType::Binary);

		[[maybe_unused]] auto divide
			= operatorAssociativity(NamedOperator::Divide, OperatorType::Binary);

		[[maybe_unused]] auto remainder
			= operatorAssociativity(NamedOperator::Remainder, OperatorType::Binary);

		auto add = operatorAssociativity(NamedOperator::Plus, OperatorType::Binary);

		[[maybe_unused]] auto subtract
			= operatorAssociativity(NamedOperator::Minus, OperatorType::Binary);

		[[maybe_unused]] auto assign
			= operatorAssociativity(NamedOperator::Assign, OperatorType::Binary);

		assertTrue(period == period_2, "Same operators have different associativity");

		assertTrue(exponentiate == OperatorAssociativity::RightToLeft, "** had bad associativity");
		assertTrue(multiply == OperatorAssociativity::LeftToRight, "* has bad associativity");
		assertTrue(add == OperatorAssociativity::LeftToRight, "* has bad associativity");

		assertTrue(inc == OperatorAssociativity::RightToLeft, "++ has bad associativity");
	}

	void exportsForLSPTest() {
		ASSERT_EQUAL_PRINT(lang_def::getKeywords().size(), 91);
		ASSERT_EQUAL_PRINT(lang_def::getSpecials().size(), 7);
		ASSERT_EQUAL_PRINT(lang_def::getOperators().size(), 43);
		ASSERT_EQUAL_PRINT(lang_def::getNumericTypeSpecifiers().size(), 15);
	}
};

TESTER_COMMON_MAIN("/src/common/lang_definitions/tests/");
