#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Struct> Struct::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Struct>(position);

		RIFT_ASSERT(
			state.ctokens().is(Keyword::Struct), position.genErrorStr("bad statement choice")
		);

		parseAll(state, Keyword::Struct, &out->name);

		constexpr auto isCurlyGroupStart = [](const RiftParserState& lstate, usize fwd) {
			return lstate.ctokens().isBracketGroup(Token::BracketType::Curly, fwd);
		};

		if (state.tryEat(Operator::Colon))
			parseList<true>(state, out->bases, Operator::Comma, isCurlyGroupStart);

		parseOne(state, &out->body);

		return out;
	}

	void Struct::dprint(std::ostream& out) const {
		out << "{\"Struct\": {\"name\":";
		nullAwareDprint(name, out);

		out << R"(,"base_classes":[)";

		for (const auto& base: bases) {
			nullAwareDprint(base, out);
			out << ", ";
		}

		out << "],";

		out << R"("body": )";
		nullAwareDprint(body, out);

		out << "}}";
	}

}
