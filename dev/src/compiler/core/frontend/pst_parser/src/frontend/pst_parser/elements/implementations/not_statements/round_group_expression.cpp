#include "../../hierarchy/not_statements/round_group_expression.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(RoundGroupExpr, expr);

	MBox<RoundGroupExpr> RoundGroupExpr::parse(LangParserState& state) {
		auto out = makeBox<RoundGroupExpr>(state);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.logInt(makeBox<RoundExprStartError>(state.getPosition()));
			return nullptr;
		}

		PARSE().goDown();
		PARSE().one(&out->expr);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void RoundGroupExpr::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	HashAlg& RoundGroupExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
