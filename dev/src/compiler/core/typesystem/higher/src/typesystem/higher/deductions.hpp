/**
 * @file deductions.hpp
 * @brief Type deduction utilities for the higher type system.
 *
 * This module provides utilities for deducing types in various contexts,
 * such as variable declarations, generic type inference, and other scenarios
 * where type information needs to be derived from expressions or other type information.
 */

#pragma once

#include "expression_type.hpp"
#include "mutability.hpp"
#include "symbol_type.hpp"

#include <concepts>

namespace compiler::tsh::deductions {
	/**
	 * @brief Deduces the symbol type of a declaration from an expression type.
	 *
	 * This function extracts the symbol type from an expression type and applies
	 * the expected mutability based on the declaration context (e.g., var/let/const).
	 * This is commonly used when inferring the type of a variable declaration from
	 * its initializer expression.
	 *
	 * @tparam ABSTRACT_TYPE The underlying abstract type class.
	 * @param expr_type The expression type of the initializer value.
	 * @param expected_mutability The mutability expected by the declaration context.
	 * @return The symbol type with the appropriate mutability applied.
	 *
	 * @example
	 * // For a declaration like: `let x = 42`
	 * // where expr_type is the type of `42` (an integral temporary value)
	 * // and expected_mutability is Immutable (from `let` keyword)
	 * auto symbol_type = deduceSymbolTypeFromExpr(expr_type, Mutability::Immutable);
	 */
	template<std::derived_from<AbstractType> ABSTRACT_TYPE = AbstractType>
	[[nodiscard]]
	SymbolType<ABSTRACT_TYPE> deduceSymbolTypeFromExpr(
		const ExpressionType<ABSTRACT_TYPE>& expr_type, const Mutability expected_mutability
	) {
		return expr_type.getSymbolType().withMutability(expected_mutability);
	}
}
