#pragma once

#include <diagnostic_interactive/stable_position.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>

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
