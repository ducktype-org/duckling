#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Struct> Struct::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Struct>(position);

		RIFT_ASSERT(state[0].is(Keyword::Struct), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Struct, &out->name);

		if (state.tryEat(Operator::Colon)) parseOne(state, &out->bases);

		parseOne(state, &out->body);

		return out;
	}

	void Struct::dprint(std::ostream& out) const {
		out << "{\"Struct\": {\"name\":";
		nullAwareDprint(name, out);

		out << R"(,"base_classes":)";
		nullAwareDprint(bases, out);
		out << ",";

		out << R"("body": )";
		nullAwareDprint(body, out);

		out << "}}";
	}

}
