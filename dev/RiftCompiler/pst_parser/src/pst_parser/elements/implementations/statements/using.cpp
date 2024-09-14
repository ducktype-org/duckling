#include "preamble.hpp"

namespace pst {
	ParserRef<Using> Using::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Using>(position);

		if (!assertStmtChoice<Using>(state, state[0].is(Keyword::Using))) return nullptr;

		state.parse(out).all(Keyword::Using, &out->names);

		return out;
	}

	void Using::dprint(std::ostream& out) const {
		out << "{\"Using\": ";
		nullAwareDprint(names, out);
		out << "}";
	}

	void Using::semPrint(std::ostream& out) const {
		out << "{\"Using\": {";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "event",)"; // TODO: maybe needs a change?
		nullAwareSemanticTokenPrint(names, out);
		out << "}}";
	}

	void Using::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitUsing(*this); }
}
