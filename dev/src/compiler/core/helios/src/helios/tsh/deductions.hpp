// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
#include "symbol_type.hpp"
#include "value_category.hpp"
#include "value_type.hpp"

namespace compiler::tsh::deductions {
	/**
	 * @brief Deduces the symbol type of a declaration from the expression type of its initialiser.
	 *
	 * This function extracts the value type from an expression type and applies
	 * the expected place access based on the declaration context (e.g., var/let/const).
	 * This is commonly used when inferring the type of a variable declaration from
	 * its initializer expression.
	 *
	 * The declaration gets the abstract type of the initializer with default symbol properties, so
	 * `var x = list.at(0)` declares a copy of the element, not an alias to it. A `ref` or `box`
	 * declaration needs the type written out (see declarationTypeFromProvidedType()).
	 *
	 * @param expr_type The expression type of the initializer value.
	 * @param place_access The place access expected by the declaration context.
	 * @return The symbol type with the appropriate place access applied.
	 *
	 * @example
	 * // For a declaration like: `let x = 42`
	 * // where expr_type is the type of `42` (an integral temporary value)
	 * // and place_access is Immutable (from `let` keyword)
	 * auto symbol_type = declarationTypeFromInitializer(expr_type, PlaceAccess::Immutable);
	 */
	[[nodiscard]]
	SymbolType declarationTypeFromInitializer(
		const ExpressionType<>& expr_type, PlaceAccess place_access
	);

	/**
	 * @brief Deduces the symbol type of a declaration from the provided type.
	 *
	 * This function combines the type provided in a declaration with the expected place access
	 * given by the declaration.
	 *
	 * @param given_type The value type given in the declaration.
	 * @param place_access The place access expected by the declaration context.
	 * @return The symbol type with the appropriate place access applied.
	 *
	 * @example
	 * // For a declaration like: `let x: i32`
	 * // the given value type is integral,
	 * // and place_access is Immutable (from `let` keyword)
	 * auto decl_type = declarationTypeFromProvidedType(given_type, PlaceAccess::Immutable);
	 */
	[[nodiscard]]
	SymbolType declarationTypeFromProvidedType(
		const ValueType<>& given_type, PlaceAccess place_access
	);
}
