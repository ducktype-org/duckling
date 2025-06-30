#pragma once

#include <helios/helios_errors.hpp>
#include <helios/hout/elements/expr.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/hierarchy/expressions/chain_expr.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * Constructs a HOUT expression from PST chain expression.
	 * Used in the process of lowering PST expressions to HOUT expressions
	 */
	query::QResult<Box<code::Expr>, errors::Failed> fromChainExpr(
		query::Context& ctx, pst::AccessLocked<pst::expr::ChainExpr> expr
	);
}
