#include "preamble.hpp"

namespace pst {
	ParserRef<ExprStmt> ExprStmt::parse(RiftParserState& state) {
		auto out = makeRef<ExprStmt>(state.getPosition());

		state.parse(out).with<Expr>(&out->expression, Expr::parse, true);

		if (out->expression == nullptr) std::cerr << "AAA\n";

		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const {
		out << "{\"Expr Stmt\" :";
		expression->dprint(out);
		out << "}";
	}

	void ExprStmt::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
