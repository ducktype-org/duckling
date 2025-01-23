#include "preamble.hpp"

namespace pst {
	MBox<Using> Using::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Using>(position);

		if (!assertStmtChoice<Using>(state, state[0].is(Keyword::Using))) return nullptr;

		state.parse(out).all(Keyword::Using, &out->names);

		return out;
	}

	void Using::dprint(std::ostream& out) const { nullAwareDprint(names, out); }

	void Using::acceptVisitor(PstVisitor& visitor) const { visitor.visitUsing(*this); }
}
