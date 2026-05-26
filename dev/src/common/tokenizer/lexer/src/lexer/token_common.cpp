#include "token_common.hpp"

#include <set>

namespace lexer {
	bool Operator::isComparison() const {
		using namespace lang_def;
		static std::set<NamedOperator> comparisons = {
			NamedOperator::Lesser, NamedOperator::LEqual, NamedOperator::Greater,
			NamedOperator::GEqual, NamedOperator::Equal,  NamedOperator::NotEqual,
		};
		return comparisons.contains(asNamed());
	}

	bool Operator::isAssignment() const { return !isComparison() && value.strView().back() == '='; }

	bool Operator::isSpecialOp() const {
		using namespace lang_def;
		static std::set<NamedOperator> specials
			= { NamedOperator::Period,      NamedOperator::PeriodStar,
			    NamedOperator::Colon,       NamedOperator::SingleArrow,
			    NamedOperator::DoubleArrow, NamedOperator::PeriodQuestion,
			    NamedOperator::Reflect };
		return specials.contains(asNamed());
	}

	bool Operator::isAccessOp() const {
		using namespace lang_def;
		static std::set<NamedOperator> access
			= { NamedOperator::Period, NamedOperator::PeriodQuestion, NamedOperator::Reflect };
		return access.contains(asNamed());
	}

	bool Operator::isNotReserved() const {
		return !isComparison() && !isAssignment() && !isSpecialOp();
	}

	base::Optional<Operator> Operator::filterNotReserved() const {
		if (isNotReserved()) return { *this };
		return {};
	}

	i64 Operator::getGenBinOpPrecedence() const {
		using namespace lang_def;
		static const std::unordered_map<lang_def::NamedOperator, i64> precedences
			= { { NamedOperator::RightShift, 510 }, { NamedOperator::LeftShift, 510 },
			    { NamedOperator::Ampersand, 520 },  { NamedOperator::BitXor, 530 },
			    { NamedOperator::Pipe, 540 },       { NamedOperator::Exponentiate, 550 },
			    { NamedOperator::Multiply, 560 },   { NamedOperator::Divide, 560 },
			    { NamedOperator::Remainder, 560 },  { NamedOperator::Plus, 570 },
			    { NamedOperator::Minus, 570 },      { NamedOperator::As, 400 } };
		if (precedences.contains(asNamed()))
			return precedences.at(asNamed());
		else
			return 500;
	}
}
