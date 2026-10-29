// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "ls_utils.hpp"

#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>

namespace compiler::helios::ls {

	CRef<query::QResult<Box<code::Expr>>> getHoutExpr(
		query::Context& ctx, pst::Access<pst::ExprElement> element
	) {
		return ctx.query<QueryHoutOfExpr>({ element });
	}

	query::QResult<SymID> getSymbolOfStmt(
		query::Context& ctx, pst::Access<pst::LangElement> element
	) {
		return ctx.query<QuerySymbolOfSTMT>({ element });
	}
}
