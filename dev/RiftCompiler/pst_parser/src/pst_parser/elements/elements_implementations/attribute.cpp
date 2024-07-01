#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	bool Attribute::trailingSemicolon() { return false; }

	ParserRef<Attribute> Attribute::parse(RiftParserState& state) {
		auto                 position = state.getPosition();
		ParserRef<Attribute> out      = makeRef<Attribute>(position);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		parseAll(state, Special::AtSign, &out->name);

		if (state[0].isBracketGroup(Token::BracketType::Round)) out->args = AtrArgList::parse(state);

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

	void Attribute::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitAttribute(*this); }
}
