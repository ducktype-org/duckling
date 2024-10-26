#include "preamble.hpp"

namespace pst {
	ParserRef<If> If::parse(LangParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<If>(position);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		state.parse(out).all(Keyword::If, &out->optional_name, &out->condition, &out->body);

		if (state.parse(out).tryEat(Keyword::Else)) state.parse(out).one(&out->else_body, true);

		return out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{\"name\":";
		nullAwareDprint(optional_name, out);
		out << ",\"condition\":";
		nullAwareDprint(condition, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << ", \"else body\": ";
		nullAwareDprint(else_body, out);
		out << "}";
	}

	void If::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitIf(*this); }
}
