#pragma once

#include <lang_definitions/key_spec_op.hpp>
#include <string_id/string_id.hpp>

namespace lexer {
	/**
	 * @brief Simple wrapper for an operator.
	 */
	struct OperatorValue final {
		const base::StrID value;

		OperatorValue() = delete;

		OperatorValue(const base::StrID id): value(id) {}

		OperatorValue(const lang_def::NamedOperator op): value(lang_def::operatorToStr(op)) {}

		OperatorValue(const OperatorValue&) = default;

		operator base::StrID() { return value; }

		[[nodiscard]]
		bool isComparison() const;

		[[nodiscard]]
		bool isAssignment() const;

		[[nodiscard]]
		bool isSpecialOp() const;

		[[nodiscard]]
		bool isNotReserved() const;

		[[nodiscard]]
		bool isAccessOp() const;

		[[nodiscard]]
		base::Optional<OperatorValue> filterNotReserved() const;

		[[nodiscard]]
		i64 getGenBinOpPrecedence() const;

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
		bool operator==(const lang_def::NamedOperator& op) const {
			return lang_def::operatorToStr(op) == value;
		}

		auto operator<=>(const OperatorValue& other) const = default;

		friend auto hashDecompose(const OperatorValue& c) { return std::tie(c.value); }
	};
}
