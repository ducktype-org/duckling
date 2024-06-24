#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	ParserRef<Variable> Variable::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Variable>(position);

		const bool is_var = state.ctokens().is(Keyword::Var);
		const bool is_let = state.ctokens().is(Keyword::Let);
		RIFT_ASSERT(is_var ^ is_let, position.genStr("bad statement choice"));
		out->is_const = is_let;

		// Todo: Add a possibility for type deduction from assigned value and no initial value.
		parseAll(state, is_var ? Keyword::Var : Keyword::Let, &out->name, Operator::Colon);
		out->type = Expr::parseUntil(state, Operator::Assign, true);
		parseAll(state, Operator::Assign);
		out->value = Expr::parse(state, true);

		return out;
	}

	void Variable::dprint(std::ostream& out) const {
		out << "{\"";
		out << (is_const ? "let" : "var");
		out << "\": {";
		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << "}}";
	}

	void Variable::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}
