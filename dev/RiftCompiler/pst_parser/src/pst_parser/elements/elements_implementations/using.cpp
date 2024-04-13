#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Using> Using::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Using>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Using), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Using, &out->names);

		return out;
	}

	void Using::dprint(std::ostream& out) const {
		out << "{\"Using\": ";
		nullAwareDprint(names, out);
		out << "}";
	}
}
