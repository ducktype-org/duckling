#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> RoundExpr::parse(LangParserState& state, i64 length) {
		std::cerr << "Parsing Round Group Expression";
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))) {}  // Error

		auto out = box<RoundExpr>(state.getPosition());

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

	void RoundExpr::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitRoundExpr(*this); }
}
