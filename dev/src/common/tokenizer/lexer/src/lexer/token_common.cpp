#include "token_common.hpp"

#include <unicode_classification/classifications.hpp>

#include <set>

namespace lexer {
	bool isOperatorSymbolString(std::string_view name) {
		using Class = unicode::Classifications;

		if (name.empty()) return false;

		const icu::UnicodeString unicode_name = icu::UnicodeString::fromUTF8(
			icu::StringPiece(name.data(), static_cast<i32>(name.size()))
		);
		const i32 length = unicode_name.length();

		i32 index = 0;
		if (!Class::operator_start.contains(unicode_name.char32At(index))) return false;
		index = unicode_name.moveIndex32(index, 1);

		while (index < length) {
			if (!Class::operator_continue.contains(unicode_name.char32At(index))) return false;
			index = unicode_name.moveIndex32(index, 1);
		}
		return true;
	}

	bool Operator::isComparison() const {
		using namespace lang_def;
		static std::set<NamedOperator> comparisons = {
			NamedOperator::Lesser, NamedOperator::LEqual, NamedOperator::Greater,
			NamedOperator::GEqual, NamedOperator::Equal,  NamedOperator::NotEqual,
		};
		return comparisons.contains(asNamed());
	}

	bool Operator::isAssignment() const { return value.strView() == "="; }

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
			= { { NamedOperator::EqPlus, 1'000 },
			    { NamedOperator::EqMinus, 1'000 },
			    { NamedOperator::EqMultiply, 1'000 },
			    { NamedOperator::EqDivide, 1'000 },
			    { NamedOperator::EqRemainder, 1'000 },
			    { NamedOperator::EqExponentiate, 1'000 },
			    { NamedOperator::EqPipe, 1'000 },
			    { NamedOperator::EqAmpersand, 1'000 },
			    { NamedOperator::EqBitXor, 1'000 },
			    { NamedOperator::EqLeftShift, 1'000 },
			    { NamedOperator::EqRightShift, 1'000 },
			    { NamedOperator::RightShift, 510 },
			    { NamedOperator::LeftShift, 510 },
			    { NamedOperator::Ampersand, 520 },
			    { NamedOperator::BitXor, 530 },
			    { NamedOperator::Pipe, 540 },
			    { NamedOperator::Exponentiate, 550 },
			    { NamedOperator::Multiply, 560 },
			    { NamedOperator::Divide, 560 },
			    { NamedOperator::Remainder, 560 },
			    { NamedOperator::Plus, 570 },
			    { NamedOperator::Minus, 570 },
			    { NamedOperator::As, 400 } };
		if (precedences.contains(asNamed()))
			return precedences.at(asNamed());
		else
			return 500;
	}
}
