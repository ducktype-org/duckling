#include "preamble.hpp"

namespace pst {
	ParserRef<ExprStmt> ExprStmt::parse(RiftParserState& state) {
		auto out = makeRef<ExprStmt>(state.getPosition());

		state.parse(out).with<Expr>(&out->expression, Expr::parse, true);

		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const {
		out << "{\"Expr Stmt\" :";
		expression->dprint(out);
		out << "}";
	}

	void ExprStmt::semPrint(std::ostream& out) const {
		out << "{\"Expr Stmt\" : {";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "method",)";  // @TODO: maybe needs a change?
		out << "\"expression\": ";
		nullAwareSemanticTokenPrint(expression, out);
		out << "}}";
	}

	void ExprStmt::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
