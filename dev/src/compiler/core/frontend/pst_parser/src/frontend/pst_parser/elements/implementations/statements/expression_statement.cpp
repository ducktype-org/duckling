// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expr_holders.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/expr_stmt.hpp"
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ExprStmt, expr);

	MBox<ExprStmt> ExprStmt::parse(LangParserState& state) {
		auto out = makeBox<ExprStmt>(state);
		PARSE().one(&out->expr);
		PST_RETURN out;
	}

	void ExprStmt::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	HashAlg& ExprStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ExprStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
