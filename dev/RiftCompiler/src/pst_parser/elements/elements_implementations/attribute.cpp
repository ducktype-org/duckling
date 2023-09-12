#include "elements_implementation.hpp"

namespace pst {
	bool Attribute::trailingSemicolon() {
		return false;
	}

	ParserRef<Attribute> Attribute::parse(RiftParserState& state){
		ParserRef<Attribute> out = makeRef<Attribute>(state.ctokens().peek().getPosition());
	
		RIFT_ASSERT(state.ctokens().is(Special::AtSign), "bad statement choice");

		parseAll(state, Special::AtSign, &out->name);

		if (state.ctokens().is(Token::Type::RoundGroup)) {
			out->args = ArgList::parse(state);
		}

		return out;
	}

	void Attribute::dprint(std::ostream& out) const {
		out << "{\"Attribute\" : {";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (args != nullptr) {
			out<<", \"args\": ";
			nullAwareDprint(args, out);
		}
		out<<"}}";
	}
}
