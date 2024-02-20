#include "elements_implementation.hpp"

namespace pst {
	ParserRef<RoundGroupExpr> RoundGroupExpr::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();

		auto out = makeRef<RoundGroupExpr>(position);

		if (!state.ctokens().isBracketGroup(Token::BracketType::Round)) {
			state.fail(-1, "expected a `(` after here");
		} else {
			state.goDown();
			if (state.notEmpty()) out->expr = Expr::parse(state);
			state.goUpAndSkip();
		}

		return out;
	}

	void RoundGroupExpr::dprint(std::ostream& out) const {
		out << "{\"RoundGroupExpr\": ";
		nullAwareDprint(expr, out);
		out << " }";
	}
}
