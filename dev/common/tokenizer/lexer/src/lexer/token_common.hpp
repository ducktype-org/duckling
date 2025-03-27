#pragma once

#include <set>

#include <base/string_id.hpp>
#include <lang_definitions/key_spec_op.hpp>

namespace lexer {
	/**
	 * @brief Simple wrapper for an operator
	 */
	struct Operator final {

		const base::StrID value;

		Operator() = delete;

		Operator(const base::StrID id): value(id) {}

		Operator(const lang_def::NamedOperator op): value(lang_def::operatorToStr(op)) {}

		Operator(const Operator&) = default;

		operator base::StrID() { return value; }

		bool isComparison() {
			using namespace lang_def;
			static std::set<NamedOperator> comparisons = {
				NamedOperator::Lesser, NamedOperator::LEqual, NamedOperator::Greater,
				NamedOperator::GEqual, NamedOperator::Equal,  NamedOperator::NotEqual,
			};
			return comparisons.contains(asNamed());
		}

		bool isAssignment() {
			return !isComparison()
			    && value.strView().back() == '=';
		}

		bool isSpecialOp() {
			using namespace lang_def;
			static std::set<NamedOperator> specials = {
				NamedOperator::Period, NamedOperator::PeriodStar, NamedOperator::Colon,
				NamedOperator::SingleArrow, NamedOperator::DoubleArrow,
			};
			return specials.contains(asNamed());
		}

		bool isNotReserved() {
			return !isComparison() && !isAssignment() && !isSpecialOp();
		}

		base::Optional<Operator> filterNotReserved() {
			if (isNotReserved()) return {*this};
			return {};
		}

		i64 getGenBinOpPrecedence() {
			using namespace lang_def;
			static const std::unordered_map<lang_def::NamedOperator, i64> precedences = {
				{ NamedOperator::RightShift, 510 },      
				{ NamedOperator::LeftShift, 510 },      
				{ NamedOperator::BitAnd, 520 },      
				{ NamedOperator::BitXor, 530 },      
				{ NamedOperator::Pipe, 540 },      
				{ NamedOperator::Exponentiate, 550 },
				{ NamedOperator::Multiply, 560 },  
				{ NamedOperator::Divide, 560 },
				{ NamedOperator::Remainder, 560 }, 
				{ NamedOperator::Plus, 570 },
				{ NamedOperator::Minus, 570 },
			};
			if (precedences.contains(asNamed())) {
				return precedences.at(asNamed());
			} else {
				return 500;
			}
		}

		[[nodiscard]]
		std::string str() const {
			return value.str();
		}

		[[nodiscard]]
		lang_def::NamedOperator asNamed() const {
			return lang_def::strAsOperator(value);
		}

		/**
		 * @note This should do the corrected UTF-8 check in the future.
		 */
		inline bool operator==(lang_def::NamedOperator& op) {
			return lang_def::operatorToStr(op) == value;
		}

		inline bool operator==(Operator& other) { return *this == other.value; }
	};

	/**
	 * @brief Simple wrapper for a value
	 */
	struct Value final {
		const base::StrID value;

		Value();

		Value(const base::StrID id): value(id) {}

		Value(const std::string& str): value(base::StrID(str.c_str())) {}

		Value(const Value&) = default;

		operator base::StrID() { return value; }

		[[nodiscard]]
		std::string str() const {
			return value.str();
		}

		/**
		 * @note This should probably do something more in the future
		 */
		inline bool operator==(Value& other) { return value == other.value; }
	};
}
