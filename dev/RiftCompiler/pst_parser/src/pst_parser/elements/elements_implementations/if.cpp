#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	ParserRef<If> If::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<If>(position);

		if (!assertStmtChoice<If>(state, state.ctokens().is(Keyword::If))) return nullptr;

		parseAll(state, Keyword::If, &out->optional_name, &out->condition, &out->body);

		return out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{\"If\": {\"name\":";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}

	void If::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitIf(*this); }
}
