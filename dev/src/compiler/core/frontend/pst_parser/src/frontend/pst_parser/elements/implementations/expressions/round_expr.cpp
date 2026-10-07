// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/round_expr.hpp"

#include "../../hierarchy/expressions/comma.hpp"
#include "../../hierarchy/expressions/unit_expr.hpp"
#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(RoundExpr, expr);

	MBox<ExprElement> RoundExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadRoundExprError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		if (state[0].getRecursive().size() == 0) return UnitExpr::parse(state);

		auto out = makeBox<RoundExpr>(state);

		PARSE().goDown();
		PARSE().with(&out->expr, Comma::parse);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void RoundExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("inner_expr": )";
		nullAwareDprint(expr, out);

		out << "}";
	}

	HashAlg& RoundExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void RoundExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitRoundExpr(*this);
	}
}
