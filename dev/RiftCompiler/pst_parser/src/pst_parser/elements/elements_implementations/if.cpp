#include "elements_implementation.hpp"

namespace pst {
	ParserRef<If> If::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<If>(position);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::If, &out->optional_name, &out->condition, &out->body);

		out->setLastToken(state.getPosition(-1));

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

}
