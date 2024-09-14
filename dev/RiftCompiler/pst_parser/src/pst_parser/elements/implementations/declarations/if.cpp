#include "preamble.hpp"

namespace pst {
	ParserRef<If> If::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<If>(position);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		state.parse(out).all(Keyword::If, &out->optional_name, &out->condition, &out->body);

		return out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{\"If\": {\"name\":";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}

	void If::semPrint(std::ostream& out) const {
		out << "{\"If\": {\"name\":";
		nullAwareSemanticTokenPrint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareSemanticTokenPrint(condition, out);
		out << ", \"body\": ";
		nullAwareSemanticTokenPrint(body, out);
		out << ",";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "keyword"}})";
	}

	void If::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitIf(*this); }
}
