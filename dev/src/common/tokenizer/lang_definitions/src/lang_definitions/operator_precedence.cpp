/**
 * @file operator_precedence.hpp
 * @author Andrzej
 */

#include "operator_precedence.hpp"

#include <base/collections/maps.hpp>
#include <base/misc/init_guard.hpp>

namespace lang_def {

	namespace {
		// this is highly ineffective but is meant as a placeholder:
		base::Map<std::pair<NamedOperator, OperatorType>, i64>                   precedence;
		base::Map<std::pair<NamedOperator, OperatorType>, OperatorAssociativity> associativity;
	}

	void operator_precedence::init() {
		SIMPLE_INIT_GUARD_BEGIN;

		// this is highly imperfect but is meant as a placeholder
		precedence.put({ NamedOperator::Period, OperatorType::Binary }, 0);

		precedence.put({ NamedOperator::DoublePlus, OperatorType::UnaryRight }, 1);
		precedence.put({ NamedOperator::Minus, OperatorType::UnaryRight }, 1);
		precedence.put({ NamedOperator::BitNot, OperatorType::UnaryRight }, 1);

		precedence.put({ NamedOperator::Exponentiate, OperatorType::Binary }, 2);

		precedence.put({ NamedOperator::Multiply, OperatorType::Binary }, 3);
		precedence.put({ NamedOperator::Divide, OperatorType::Binary }, 3);
		precedence.put({ NamedOperator::Remainder, OperatorType::Binary }, 3);

		precedence.put({ NamedOperator::Plus, OperatorType::Binary }, 4);
		precedence.put({ NamedOperator::Minus, OperatorType::Binary }, 4);

		precedence.put({ NamedOperator::LeftShift, OperatorType::Binary }, 5);
		precedence.put({ NamedOperator::RightShift, OperatorType::Binary }, 5);


		precedence.put({ NamedOperator::Ampersand, OperatorType::Binary }, 6);
		precedence.put({ NamedOperator::BitXor, OperatorType::Binary }, 7);
		precedence.put({ NamedOperator::Pipe, OperatorType::Binary }, 8);

		precedence.put({ NamedOperator::Assign, OperatorType::Binary }, 9);

		associativity.put(
			{ NamedOperator::Period, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ NamedOperator::DoublePlus, OperatorType::UnaryRight },
			OperatorAssociativity::RightToLeft
		);
		associativity.put(
			{ NamedOperator::Minus, OperatorType::UnaryRight }, OperatorAssociativity::RightToLeft
		);
		associativity.put(
			{ NamedOperator::BitNot, OperatorType::UnaryRight }, OperatorAssociativity::RightToLeft
		);
		associativity.put(
			{ NamedOperator::Exponentiate, OperatorType::Binary }, OperatorAssociativity::RightToLeft
		);

		associativity.put(
			{ NamedOperator::Multiply, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::Divide, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::Remainder, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ NamedOperator::Plus, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::Minus, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ NamedOperator::LeftShift, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::RightShift, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ NamedOperator::Ampersand, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::BitXor, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);
		associativity.put(
			{ NamedOperator::Pipe, OperatorType::Binary }, OperatorAssociativity::LeftToRight
		);

		associativity.put(
			{ NamedOperator::Assign, OperatorType::Binary }, OperatorAssociativity::RightToLeft
		);

		SIMPLE_INIT_GUARD_END;
	}

	i64 operatorPrecedence([[maybe_unused]] base::StrID operatorr, OperatorType operator_type) {
		CORE_ASSERT(operatorr.isGood(), "Bad string passed to operator precedence");

		if (precedence.contains({ strAsOperator(operatorr), operator_type }))
			return precedence[{ strAsOperator(operatorr), operator_type }];

		// generic rules:
		base::RawView str_view = operatorr.view();
		CORE_ASSERT(str_view.size() > 0, "String of size zero passed to operator precedence");

		throw base::NotYetImplemented(base::strConcat(
			"Generic rules for operator precedence dont yet exist for operator: ", operatorr
		));
	}

	i64 operatorPrecedence(NamedOperator operatorr, OperatorType operator_type) {
		return operatorPrecedence(operatorToStr(operatorr), operator_type);
	}

	OperatorAssociativity operatorAssociativity(base::StrID operatorr, OperatorType operator_type) {
		CORE_ASSERT(operatorr.isGood(), "Bad string passed to operator precedence");

		if (associativity.contains({ strAsOperator(operatorr), operator_type }))
			return associativity[{ strAsOperator(operatorr), operator_type }];

		// generic rules:
		base::RawView str_view = operatorr.view();
		CORE_ASSERT(str_view.size() > 0, "String of size zero passed to operator precedence");

		throw base::NotYetImplemented(base::strConcat(
			"Generic rules for operator associativity dont yet exist for operator: ", operatorr
		));
	}

	OperatorAssociativity operatorAssociativity(NamedOperator keyword, OperatorType operator_type) {
		return operatorAssociativity(operatorToStr(keyword), operator_type);
	}
}
