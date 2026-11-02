// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::ls {
	/**
	 * @brief Gets HOUT expression corresponding to a given PST expression.
	 * Executes the query under the hood.
	 */
	CRef<query::QResult<Box<code::Expr>>> getHoutExpr(
		query::Context& ctx, pst::Access<pst::ExprElement> element
	);

	/**
	 * @brief Get SymID of a given PST element.
	 * Executes the query under the hood.
	 */
	query::QResult<SymID> getSymbolOfStmt(query::Context& ctx, pst::Access<pst::LangElement> element);
}
