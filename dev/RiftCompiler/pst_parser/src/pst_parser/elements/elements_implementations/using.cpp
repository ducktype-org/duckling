#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Using> Using::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Using>(position);

		RIFT_ASSERT(state[0].is(Keyword::Using), position.genStr("bad statement choice"));

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::Using, &out->names);

		out->setLastToken(state.getPosition(-1));

		return out;
	}

	void Using::dprint(std::ostream& out) const {
		out << "{\"Using\": ";
		nullAwareDprint(names, out);
		out << "}";
	}
}
