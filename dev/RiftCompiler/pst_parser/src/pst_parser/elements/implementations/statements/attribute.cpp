#include "preamble.hpp"

namespace pst {
	bool Attribute::trailingSemicolon() { return false; }

	ParserRef<Attribute> Attribute::parse(RiftParserState& state) {
		auto                 position = state.getPosition();
		ParserRef<Attribute> out      = makeRef<Attribute>(position);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		state.parse(out).all(Special::AtSign, &out->name);

		if (state[0].isBracketGroup(Token::BracketType::Round)) state.parse(out).one(&out->args);

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

	void Attribute::semPrint(std::ostream& out) const {
		out << "{\"Attribute\" : {";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "property",)";  // TODO: maybe needs a change?
		out << "\"name\" : ";
		nullAwareSemanticTokenPrint(name, out);
		if (args != nullptr) {
			out << ", \"args\": ";
			nullAwareSemanticTokenPrint(args, out);
		}
		out << "}}";
	}

	void Attribute::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitAttribute(*this); }
}
