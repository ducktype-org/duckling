#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/match_expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::desugaring {
	/**
	 * @brief Desugars a `match` expression into a `code::MatchExpr`.
	 *
	 * Every case has to evaluate to the same type, which becomes the type of the whole
	 * expression. Currently supported patterns: `case x : T`, `case x : ref T`, `case _ : T`
	 * and `case _`.
	 *
	 * @return The match expression, or a failure with diagnostics already logged.
	 */
	query::QResult<Box<code::Expr>> desugarMatch(
		query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
	);
}
