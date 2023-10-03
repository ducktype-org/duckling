#include "elements_implementation.hpp"

namespace pst {
	ParserRef<If> If::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<If>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::If), position.genErrorMsg("bad statement choice"));

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

}  // namespace pst
