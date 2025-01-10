#include "preamble.hpp"

#include "../../hierarchy/expr.hpp"

namespace pst {
	MBox<ExprStmt> ExprStmt::parse(LangParserState& state) {
		auto out = makeBox<ExprStmt>(state.getPosition());
		state.parse(out)
			.with(&out->expr, expr::parseUntil<expr::Assignment, ExprClassify::exprStmtEnd>);
		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	void ExprStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
