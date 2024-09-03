#include "preamble.hpp"

namespace pst {
	ParserRef<ExprStmt> ExprStmt::parse(DucklingParserState& state) {
		auto out = makeRef<ExprStmt>(state.getPosition());

		state.parse(out).with<Expr>(&out->expression, Expr::parse, true);

		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const {
		out << "{\"Expr Stmt\" :";
		expression->dprint(out);
		out << "}";
	}

	void ExprStmt::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
