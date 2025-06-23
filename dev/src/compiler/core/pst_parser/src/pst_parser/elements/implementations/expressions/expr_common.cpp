#include "../../hierarchy/expressions/expr_common.hpp"

#include "preamble.hpp"

namespace pst {
	bool ExprClassify::isComparison(const LangParserState& state, i64 fwd) {
		return state[fwd].asBinaryOperator().map([](auto op) { return op.isComparison(); }
		).valueOr(false);
	}

	bool ExprClassify::isAssignment(const LangParserState& state, i64 fwd) {
		return state[fwd].asBinaryOperator().map([](auto op) { return op.isAssignment(); }
		).valueOr(false);
	}

	bool ExprClassify::exprStmtEnd(const LangParserState& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon);
	}
}
