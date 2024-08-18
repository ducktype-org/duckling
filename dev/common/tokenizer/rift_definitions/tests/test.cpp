#include <tester/tester.hpp>
#include <rift_definitions/key_spec_op.hpp>
#include <rift_definitions/operator_precedence.hpp>

class SimpleRiftDefTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleRiftDefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple rift definitions test") {
		rift_def::key_spec_op::init();
		rift_def::operator_precedence::init();

		TESTER_ADD_TEST(simpleOperatorPrecedenceTest);
		TESTER_ADD_TEST(simpleOperatorAssociativityTest);
		TESTER_ADD_TEST(exportsForLSPTest);
	}

private:
	void simpleOperatorPrecedenceTest() {
		using namespace rift_def;

		auto period   = operatorPrecedence(base::StrId("."), OperatorType::Binary);
		auto period_2 = operatorPrecedence(Operator::Period, OperatorType::Binary);

		auto inc = operatorPrecedence(Operator::DoublePlus, OperatorType::UnaryRight);
		auto neg = operatorPrecedence(Operator::Minus, OperatorType::UnaryRight);

		auto multiply = operatorPrecedence(Operator::Multiply, OperatorType::Binary);
		auto divide   = operatorPrecedence(Operator::Divide, OperatorType::Binary);

		auto add      = operatorPrecedence(Operator::Plus, OperatorType::Binary);
		auto subtract = operatorPrecedence(Operator::Minus, OperatorType::Binary);

		auto assign = operatorPrecedence(Operator::Assign, OperatorType::Binary);

		assert(period == period_2, "Same operators have different precedence");
		assert(multiply == divide, "*, / have different precedence");
		assert(add == subtract, "+, - have different precedence");

		assert(period < add, ". is not the first operator");
		assert(period < inc, ". is not the first operator");
		assert(period < neg, ". is not the first operator");
		assert(period < subtract, ". is not the first operator");
		assert(period < multiply, ". is not the first operator");
		assert(period < divide, ". is not the first operator");
		assert(period < assign, ". is not the first operator");

		assert(multiply < subtract, "* is not before -");
	}

	void simpleOperatorAssociativityTest() {
		using namespace rift_def;

		auto period   = operatorAssociativity(base::StrId("."), OperatorType::Binary);
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

		assert(period == period_2, "Same operators have different associativity");

		assert(multiply == OperatorAssociativity::LeftToRight, "* has bad associativity");
		assert(add == OperatorAssociativity::LeftToRight, "* has bad associativity");

		assert(inc == OperatorAssociativity::RightToLeft, "++ has bad associativity");
	}

	void exportsForLSPTest() {
		ASSERT_EQUAL(rift_def::getKeywords().size(), 60);
		ASSERT_EQUAL(rift_def::getSpecials().size(), 6);
		ASSERT_EQUAL(rift_def::getOperators().size(), 15);
	}
};

TESTER_COMMON_MAIN("/common/rift_definitions/tests/");
