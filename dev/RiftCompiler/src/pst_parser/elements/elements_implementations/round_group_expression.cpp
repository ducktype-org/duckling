#include "elements_implementation.hpp"

namespace pst {
	ParserRef<RoundGroupExpr> RoundGroupExpr::parse(RiftParserState& state) {
		auto out = makeRef<RoundGroupExpr>(state.ctokens().peek().getPosition());

		if (!state.ctokens().is(Token::Type::RoundGroup)) {
			state.fail(-1, "expected a `(` after here");
		}
		else {
			state.goDown();
			if (state.notEmpty()) {
				out->expr = Expr::parse(state);
			}
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
