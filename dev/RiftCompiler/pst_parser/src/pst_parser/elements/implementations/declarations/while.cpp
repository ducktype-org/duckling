#include "preamble.hpp"

namespace pst {
	ParserRef<While> While::parse(RiftParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<While>(position);

		if (!assertStmtChoice<While>(state, state[0].is(Keyword::While))) return nullptr;

		state.parse(out).all(Keyword::While, &out->optional_name, &out->condition, &out->body);

		return out;
	}

	void While::dprint(std::ostream& out) const {
		out << R"({"While": {"name":)";
		nullAwareDprint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareDprint(condition, out);
		out << ", \"body\": ";
		nullAwareDprint(body, out);
		out << "}}";
	}

	void While::semPrint(std::ostream& out) const {
		out << R"({"While": {"name":)";
		nullAwareSemanticTokenPrint(optional_name, out);
		out << ", \"condition\": ";
		nullAwareSemanticTokenPrint(condition, out);
		out << ", \"body\": ";
		nullAwareSemanticTokenPrint(body, out);
		out << ",";
		getSourcePosition().semPrint(out);
		out << ",\"semanticTokenType\": \"keyword\"}}";
	}

	void While::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitWhile(*this); }
}
