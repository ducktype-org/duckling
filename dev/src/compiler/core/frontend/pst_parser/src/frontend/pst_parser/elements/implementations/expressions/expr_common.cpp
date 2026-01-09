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
