// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "token_common.hpp"

#include <unicode_classification/classifications.hpp>

#include <set>

namespace lexer {
	namespace {
		/**
		 * @brief Checks whether `name` is non-empty, starts with a character from `start`, and
		 * continues with characters from `continuation`.
		 */
		bool matchesCharacterClasses(
			std::string_view name, const icu::UnicodeSet& start, const icu::UnicodeSet& continuation
		) {
			if (name.empty()) return false;

			const icu::UnicodeString unicode_name = icu::UnicodeString::fromUTF8(
				icu::StringPiece(name.data(), static_cast<i32>(name.size()))
			);
			const i32 length = unicode_name.length();

			i32 index = 0;
			if (!start.contains(unicode_name.char32At(index))) return false;
			index = unicode_name.moveIndex32(index, 1);

			while (index < length) {
				if (!continuation.contains(unicode_name.char32At(index))) return false;
				index = unicode_name.moveIndex32(index, 1);
			}
			return true;
		}
	}

	bool isOperatorSymbolString(std::string_view name) {
		using Class = unicode::Classifications;
		return matchesCharacterClasses(name, Class::operator_start, Class::operator_continue);
	}

	bool isIdentifierString(std::string_view name) {
		using Class = unicode::Classifications;
		return matchesCharacterClasses(name, Class::name_start, Class::name_continue);
	}

	bool Operator::isComparison() const {
		using namespace lang_def;
		static std::set<NamedOperator> comparisons = {
			NamedOperator::Lesser, NamedOperator::LEqual, NamedOperator::Greater,
			NamedOperator::GEqual, NamedOperator::Equal,  NamedOperator::NotEqual,
		};
		return comparisons.contains(asNamed());
	}

	// @TODO: #3702 Handle custom operators ending with `=`
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

	bool Operator::isNotReserved() const { return !isAssignment() && !isSpecialOp(); }

	base::Optional<Operator> Operator::filterNotReserved() const {
		if (isNotReserved()) return { *this };
		return {};
	}

	bool Operator::isGenBinOp() const {
		return isNotReserved() || asNamed() == lang_def::NamedOperator::SingleArrow;
	}

	i64 Operator::getGenBinOpPrecedence() const {
		using namespace lang_def;
		static const std::unordered_map<lang_def::NamedOperator, i64> precedences
			= { { NamedOperator::RightShift, 510 }, { NamedOperator::LeftShift, 510 },
			    { NamedOperator::Ampersand, 520 },  { NamedOperator::BitXor, 530 },
			    { NamedOperator::Pipe, 540 },       { NamedOperator::Exponentiate, 550 },
			    { NamedOperator::Multiply, 560 },   { NamedOperator::Divide, 560 },
			    { NamedOperator::Remainder, 560 },  { NamedOperator::Plus, 570 },
			    { NamedOperator::Minus, 570 },      { NamedOperator::SingleArrow, 580 },
			    { NamedOperator::As, 400 } };
		// Identifier operators (e.g. `a contains b`) bind looser than every symbolic operator.
		static constexpr i64 IDENTIFIER_OPERATOR_PRECEDENCE = 600;
		static constexpr i64 CUSTOM_OPERATOR_PRECEDENCE     = 500;

		if (precedences.contains(asNamed())) return precedences.at(asNamed());
		if (!isOperatorSymbolString(value.strView())) return IDENTIFIER_OPERATOR_PRECEDENCE;
		return CUSTOM_OPERATOR_PRECEDENCE;
	}

	bool Operator::isGenBinOpRightAssociative() const {
		return asNamed() == lang_def::NamedOperator::SingleArrow;
	}
}
