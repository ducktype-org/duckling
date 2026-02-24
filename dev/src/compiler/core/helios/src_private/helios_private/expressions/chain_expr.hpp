#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/chain_expr.hpp>
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_result.hpp>
#include "frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp"

namespace compiler::helios::code {
	/**
	 * @brief Converts a PST ChainExpr to a HOUT Expr.
	 */
	query::QResult<Box<code::Expr>> fromChainExpr(
		query::Context& ctx, pst::AccessLocked<pst::expr::ChainExpr> expr
	);

	/**
	 * @brief Converts a PST IdentifierLiteral to a HOUT Expr.
	 */
	query::QResult<Box<code::Expr>> fromIdentifierLiteral(
		query::Context& ctx, pst::AccessLocked<pst::expr::IdentifierLiteral> expr
	);
}
