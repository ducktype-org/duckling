#include "../../hierarchy/not_statements/round_group_expression.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<RoundGroupExpr> RoundGroupExpr::parse(LangParserState& state) {
		auto out = makeBox<RoundGroupExpr>(state);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.logInt(makeBox<RoundExprStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();
		if (state.notEmpty()) state.parse(out).one(&out->expr);
		state.parse(out).goUpAndSkip();

		PST_RETURN out;
	}

	void RoundGroupExpr::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	LangElement::HashAlg& RoundGroupExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
