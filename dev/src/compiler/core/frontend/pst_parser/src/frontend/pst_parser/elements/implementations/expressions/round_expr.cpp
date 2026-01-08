#include "../../hierarchy/expressions/round_expr.hpp"

#include "../../hierarchy/expressions/comma.hpp"
#include "../../hierarchy/expressions/unit_expr.hpp"
#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> RoundExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadRoundExprError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		if (state[0].getRecursive().size() == 0) return UnitExpr::parse(state, length);

		auto out = makeBox<RoundExpr>(state.getPosition());

		state.parse(out).goDown();
		state.parse(out).with(&out->expr, Comma::parse, (i64) state.ctokens().size());
		state.parse(out).goUpAndSkip();

		return out;
	}

	void RoundExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("inner_expr": )";
		nullAwareDprint(expr, out);

		out << "}";
	}

	LangElement::HashAlg& RoundExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void RoundExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitRoundExpr(*this);
	}
}
