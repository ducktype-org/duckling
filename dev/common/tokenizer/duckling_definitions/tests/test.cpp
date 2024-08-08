#include <tester/tester.hpp>
#include <duckling_definitions/key_spec_op.hpp>
#include <duckling_definitions/operator_precedence.hpp>

class SimpleDucklingDefTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleDucklingDefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple duckling definitions test") {
		duckling_def::key_spec_op::init();
		duckling_def::operator_precedence::init();

		TESTER_ADD_TEST(simpleOperatorPrecedenceTest);
		TESTER_ADD_TEST(simpleOperatorAssociativityTest);
	}

private:
	void simpleOperatorPrecedenceTest() {
		using namespace duckling_def;

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
		using namespace duckling_def;

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
};

TESTER_COMMON_MAIN("/common/duckling_definitions/tests/");
