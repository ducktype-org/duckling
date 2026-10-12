// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/unit_expr.hpp"

#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> UnitExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))
		    || !state[0].getRecursive().empty()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadUnitExprError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<UnitExpr>(state);

		PARSE().goDown();
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void UnitExpr::dprint(std::ostream& out) const {
		out << "{";
		out << "}";
	}

	HashAlg& UnitExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void UnitExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitUnitExpr(*this);
	}
}
