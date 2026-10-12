// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lang_definitions/key_spec_op.hpp>
#include <string_id/string_id.hpp>

#include <string_view>

namespace lexer {
	/**
	 * @brief Checks whether `name` is entirely composed of operator characters (per the same
	 * `operator_start`/`operator_continue` character classes the lexer itself uses), as opposed to
	 * e.g. a plain identifier.
	 *
	 * @note Unlike `Operator::asNamed()`/`strAsOperator`, this recognizes *any* legal operator
	 * shape, not just the fixed set of named operators — it's the only way to tell a custom
	 * operator name (like `+*`) apart from a plain identifier once only the raw string is
	 * available (no token/lexer state).
	 */
	[[nodiscard]]
	bool isOperatorSymbolString(std::string_view name);

	/**
	 * @brief Simple wrapper for an operator.
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

		[[nodiscard]]
		lang_def::Keyword asKeyword() const {
			return lang_def::strAsKeyword(value);
		}

		/**
		 * @note This should do the corrected UTF-8 check in the future.
		 */
		bool operator==(const lang_def::NamedOperator& op) const {
			return lang_def::operatorToStr(op) == value;
		}

		auto operator<=>(const Operator& other) const = default;

		friend auto hashDecompose(const Operator& c) { return std::tie(c.value); }
	};
}
