#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/match_expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context.hpp>

#include <functional>

namespace compiler::helios::desugaring {
	/**
	 * @brief Builds the statement a match case's result feeds into.
	 * Receives the case result already coerced to the expected type.
	 */
	using MatchResultSink = std::function<Box<code::Stmt>(BoxOrCRef<code::Expr> case_result)>;

	/**
	 * @brief Desugars a `match` expression used at the statement surface.
	 *
	 * Produces a BlockStmt wrapping a code::MatchStmt whose case bodies feed the coerced
	 * case results into @p sink.
	 * Currently supported patterns: `case x : T`, `case _ : T` and `case _`.
	 *
	 * @return The desugared block on success, an empty optional on failure
	 * (diagnostics are logged).
	 */
	base::Optional<code::BlockStmt> desugarMatch(
		query::Context&                   ctx,
		pst::Access<pst::expr::MatchExpr> match_expr,
		tsh::SymbolType<>                 expected_type,
		const MatchResultSink&            sink
	);
}
