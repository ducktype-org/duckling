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

namespace compiler::tsh::deductions {
	/**
	 * @brief Deduces the symbol type of a declaration from the expression type of its initialiser.
	 *
	 * This function extracts the symbol type from an expression type and applies
	 * the expected mutability based on the declaration context (e.g., var/let/const).
	 * This is commonly used when inferring the type of a variable declaration from
	 * its initializer expression.
	 *
	 * @param expr_type The expression type of the initializer value.
	 * @param expected_mutability The mutability expected by the declaration context.
	 * @return The symbol type with the appropriate mutability applied.
	 *
	 * @example
	 * // For a declaration like: `let x = 42`
	 * // where expr_type is the type of `42` (an integral temporary value)
	 * // and expected_mutability is Immutable (from `let` keyword)
	 * auto symbol_type = declarationTypeFromInitializer(expr_type, Mutability::Immutable);
	 */
	[[nodiscard]]
	SymbolType<> declarationTypeFromInitializer(
		const ExpressionType<>& expr_type, Mutability expected_mutability
	);

	/**
	 * @brief Deduces the symbol type of a declaration from the provided type.
	 *
	 * This function modifies the type provided in a declaration, taking into account the expected
	 * mutability given by the declaration.
	 *
	 * @param given_type The symbol type given in the declaration.
	 * @param expected_mutability The mutability expected by the declaration context.
	 * @return The symbol type with the appropriate mutability applied.
	 *
	 * @example
	 * // For a declaration like: `let x: i32`
	 * // the given symbol_type is integral, with some irrelevant mutability,
	 * // and expected_mutability is Immutable (from `let` keyword)
	 * auto decl_type = declarationTypeFromProvidedType(given_type, Mutability::Immutable);
	 */
	[[nodiscard]]
	SymbolType<> declarationTypeFromProvidedType(
		const SymbolType<>& given_type, Mutability expected_mutability
	);
}
