#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Namespace> Namespace::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Namespace>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Namespace),
		            position.genErrorMsg("bad statement choice"));

		parseAll(state, Keyword::Namespace, &out->name, &out->body);

		return out;
	}

	void Namespace::dprint(std::ostream& out) const {
		out << "{\"Namespace\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);

		// @TODO: change to body in print:
		out << R"(, "block": )";
		nullAwareDprint(body, out);
		out << "}}";
	}

}
