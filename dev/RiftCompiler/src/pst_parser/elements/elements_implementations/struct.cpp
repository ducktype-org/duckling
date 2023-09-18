#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Struct> Struct::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Struct>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Struct),
		            position.genErrorMsg("bad statement choice"));

		parseAll(state, Keyword::Struct, &out->name);

		if (state.tryEat(Operator::Colon)) {
			parseList<true>(state, out->bases, Operator::Comma, Token::Type::CurlyGroup);
		}

		parseOne(state, &out->body);

		return out;
	}

	void Struct::dprint(std::ostream& out) const {
		out << "{\"Struct\": {\"name\":";
		nullAwareDprint(name, out);

		out << R"(,"base_classes":[)";

		for (const auto& base : bases) {
			nullAwareDprint(base, out);
			out << ", ";
		}

		out << "],";

		out << R"("body": )";
		nullAwareDprint(body, out);

		out << "}}";
	}

}
