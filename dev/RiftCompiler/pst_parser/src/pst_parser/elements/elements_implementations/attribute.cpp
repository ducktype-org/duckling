#include "elements_implementation.hpp"

namespace pst {
	bool Attribute::trailingSemicolon() { return false; }

	ParserRef<Attribute> Attribute::parse(RiftParserState& state) {
		auto                 position = state.getPosition();
		ParserRef<Attribute> out      = makeRef<Attribute>(position);

		RIFT_ASSERT(state[0].is(Special::AtSign), position.genStr("bad statement choice"));

		parseAll(state, Special::AtSign, &out->name);

		if (state[0].isBracketGroup(Token::BracketType::Round)) out->args = ArgList::parse(state);

		out->setLastToken(state.getPosition(-1));

		return out;
	}

	void Attribute::dprint(std::ostream& out) const {
		out << "{\"Attribute\" : {";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (args != nullptr) {
			out << ", \"args\": ";
			nullAwareDprint(args, out);
		}
		out << "}}";
	}
}
