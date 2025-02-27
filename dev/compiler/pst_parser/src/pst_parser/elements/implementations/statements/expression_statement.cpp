#include "preamble.hpp"

namespace pst {
	MBox<ExprStmt> ExprStmt::parse(LangParserState& state) {
		auto out = makeBox<ExprStmt>(state.getPosition());
		state.parse(out).one(&out->expr);
		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	void ExprStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
