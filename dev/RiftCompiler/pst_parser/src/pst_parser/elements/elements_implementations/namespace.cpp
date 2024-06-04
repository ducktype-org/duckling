#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Namespace> Namespace::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Namespace>(position);

		RIFT_ASSERT(state[0].is(Keyword::Namespace), position.genStr("bad statement choice"));

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::Namespace, &out->name, &out->body);

		out->setLastToken(state.getPosition(-1));

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
