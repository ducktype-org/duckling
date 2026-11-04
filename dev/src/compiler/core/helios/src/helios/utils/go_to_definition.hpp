// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/hout/elements/expr.hpp>

#include <base/collections/optional.hpp>

namespace compiler::helios {
	/**
	 * Perform go-to definition on the given HOUT expression.
	 * Returns SymID that the expression is pointing to.
	 */
	base::Optional<SymID> querySymIDOfHOUTExpr(query::Context& ctx, CRef<code::Expr> expr);

	/**
	 * Perform go-to definition on the given PST-expression.
	 * Returns SymID that the expression is pointing to.
	 */
	base::Optional<SymID> querySymIDOfPSTExpr(
		query::Context& ctx, pst::AccessLocked<pst::ExprElement> expr
	);
}
