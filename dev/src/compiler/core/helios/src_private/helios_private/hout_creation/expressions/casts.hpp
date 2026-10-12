// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <diagnostic/stable_position.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Performs the coercion of @p value to @p as_type
	 * throws a query failed error if it fails.
	 */
	Box<Expr> castAs(
		query::Context&                        ctx,
		Box<Expr>                              value,
		tsh::SymbolType<>                      as_type,
		pst::Access<pst::expr::BinaryOperator> stmt
	);
}
