// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/expr_common.hpp"

#include "preamble.hpp"

namespace pst {
	bool ExprClassify::isComparison(const TokenStream& state, i64 fwd) {
		return state[fwd].asBinaryOperator().map([](auto op) { return op.isComparison(); }
		).copyValueOr(false);
	}

	bool ExprClassify::isAssignment(const TokenStream& state, i64 fwd) {
		return state[fwd].asBinaryOperator().map([](auto op) { return op.isAssignment(); }
		).copyValueOr(false);
	}

	bool ExprClassify::exprStmtEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon);
	}
}
