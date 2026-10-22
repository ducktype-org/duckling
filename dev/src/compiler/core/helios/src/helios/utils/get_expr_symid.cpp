// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "get_expr_symid.hpp"

#include <helios/hout/visitors.hpp>

#include <base/collections/optional.hpp>

namespace compiler::helios {


	struct HoutExprSymbolVisitor final: public code::HoutExprVisitorEmpty {
		base::Optional<SymID> symbol;

		void visitIdentifierExpr(const code::IdentifierExpr& val) override { symbol = val.symbol; }
	};

	/**
	 * Wrapper around the HoutExprSymbolVisitor to get the symbol ID from an expression.
	 */
	base::Optional<SymID> getIdentifierExprSymID(CRef<code::Expr> expr) {
		HoutExprSymbolVisitor visitor;
		expr->acceptVisitor(visitor);
		return visitor.symbol;
	}
}
