/**
 * @file operator_precedence.hpp
 * @author Andrzej
 */

#include "operator_precedence.hpp"
#include <base/init_guard.hpp>
#include <base/maps.hpp>

namespace rift_def {

	namespace {
		// this is highly ineffective but is meant as a placeholder:
		base::Map<std::pair<Operator, OperatorType>, usize>                 precedence;
		base::Map<std::pair<Operator, OperatorType>, OperatorAssociativity> associativity;
	}

	void operator_precedence::init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN;

		key_spec_op::init();

		// this is highly imperfect but is meant as a placeholder
		precedence.put({ Operator::Period, OperatorType::Binary }, 0);

		precedence.put({ Operator::DoublePlus, OperatorType::UnaryRight }, 1);
		precedence.put({ Operator::Minus, OperatorType::UnaryRight }, 1);

		precedence.put({ Operator::Multiply, OperatorType::Binary }, 2);
		precedence.put({ Operator::Divide, OperatorType::Binary }, 2);

		precedence.put({ Operator::Plus, OperatorType::Binary }, 3);
		precedence.put({ Operator::Minus, OperatorType::Binary }, 3);

		precedence.put({ Operator::Assign, OperatorType::Binary }, 4);

		associativity.put(
			{ Operator::Period, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ Operator::DoublePlus, OperatorType::UnaryRight }, OperatorAssociativity::RightToLeft
		);
		associativity.put(
			{ Operator::Minus, OperatorType::UnaryRight }, OperatorAssociativity::RightToLeft
		);

		associativity.put(
			{ Operator::Multiply, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ Operator::Divide, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ Operator::Plus, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ Operator::Minus, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ Operator::Assign, OperatorType::Binary }, OperatorAssociativity::RightToLeft
		);

		RIFT_SIMPLE_INIT_GUARD_END;
	}

	i64 operatorPrecedence([[maybe_unused]] base::StrId operator_, OperatorType operator_type) {
		RIFT_ASSERT(operator_.isGood(), "Bad string passed to operator precedence");

		if (precedence.contains({ strAsOperator(operator_), operator_type }))
			return precedence[{ strAsOperator(operator_), operator_type }];

		// generic rules:
		base::RawView str_view = operator_.view();
		RIFT_ASSERT(str_view.size() > 0, "String of size zero passed to operator precedence");

		throw base::NotYetImplemented(base::strConcat(
			"Generic rules for operator precedence dont yet exist for operator: ", operator_
		));
	}

	i64 operatorPrecedence(Operator operator_, OperatorType operator_type) {
		return operatorPrecedence(operatorToStr(operator_), operator_type);
	}

	OperatorAssociativity operatorAssociativity(base::StrId operator_, OperatorType operator_type) {
		RIFT_ASSERT(operator_.isGood(), "Bad string passed to operator precedence");

		if (associativity.contains({ strAsOperator(operator_), operator_type }))
			return associativity[{ strAsOperator(operator_), operator_type }];

		// generic rules:
		base::RawView str_view = operator_.view();
		RIFT_ASSERT(str_view.size() > 0, "String of size zero passed to operator precedence");

		throw base::NotYetImplemented(base::strConcat(
			"Generic rules for operator associativity dont yet exist for operator: ", operator_
		));
	}

	OperatorAssociativity operatorAssociativity(Operator keyword, OperatorType operator_type) {
		return operatorAssociativity(operatorToStr(keyword), operator_type);
	}
}
