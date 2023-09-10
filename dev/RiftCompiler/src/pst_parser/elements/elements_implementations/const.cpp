#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Const> Const::parse(RiftParserState& state) {
		auto out = makeRef<Const>();
		RIFT_ASSERT(state.ctokens().is(Keyword::Const), "bad statement choice");

		parseAll(state, Keyword::Const, &out->name, Operator::Colon);
		out->type = Expr::parseUntil(state, Operator::Assign);
		parseAll(state, Operator::Assign, &out->value);

		return out;
	}

	void Const::dprint(std::ostream& out) const {
		out << "{\"Const\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << "}}";
	}
}