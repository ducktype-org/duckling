// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Parses a PST numeric literal expression into a compile-time numeric value.
	 * Handles different bases (decimal, hex, binary, octal), type specifiers (e.g., i32,
	 * f64), and type deduction for literals without an explicit type.
	 *
	 * @note: For a literal without an explicit type specifier, returns a numeric value containing
	 * the minimal type in which a value can be stored. For example, for `40000` it will return a
	 * numeric value containing the smallest possible type `i32`.
	 * @note: For now, the minimal type used in type deduction is `i32`, meaning a value of `256`
	 * will be stored in a `i32` type, although it fits in `i16`).
	 *
	 * @param ctx The query context for logging errors.
	 * @param literal_expr An PST expression representing the numeric literal.
	 * @return An optional containing the parsed numeric value, or an empty optional if parsing
	 * failed. Errors are logged to the context.
	 */
	base::Optional<numeric_value::NumericValue> fromExprNumericValue(
		query::Context& ctx, pst::AccessLocked<pst::expr::ExprNumericValue> literal_expr
	);
}
