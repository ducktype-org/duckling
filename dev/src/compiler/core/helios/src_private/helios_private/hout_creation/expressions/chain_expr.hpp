// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/chain_expr.hpp>
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Converts a PST ChainExpr to a HOUT Expr.
	 */
	query::QResult<Box<code::Expr>> fromChainExpr(
		query::Context& ctx, pst::AccessLocked<pst::expr::ChainExpr> expr
	);

	query::QResult<Box<code::Expr>> fromIdentifierLiteral(
		query::Context& ctx, pst::AccessLocked<pst::expr::IdentifierLiteral> expr
	);
}
