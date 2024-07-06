#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	ParserRef<Const> Const::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Const>(position);

		if (!assertStmtChoice<Const>(state, state.ctokens().is(Keyword::Const))) return nullptr;

		parseAll(state, Keyword::Const, &out->name, Operator::Colon);
		out->type = Expr::parseUntil(state, Operator::Assign, true);
		parseAll(state, Operator::Assign);
		out->value = Expr::parse(state, true);

		return out;
	}

	void Const::dprint(std::ostream& out) const {
		out << "{\"Const\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "value": )";
		nullAwareDprint(value, out);
		out << "}}";
	}

	void Const::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitConst(*this); }
}
