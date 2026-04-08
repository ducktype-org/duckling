#pragma once

#include <lang_definitions/key_spec_op.hpp>
#include <string_id/string_id.hpp>

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
		base::Optional<Operator> filterNotReserved() const;

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

		auto operator<=>(const Operator& other) const = default;

		friend auto hashDecompose(const Operator& c) {
			return std::tie(c.value);
		}
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
		bool operator==(Value& other) { return value == other.value; }
	};
}
