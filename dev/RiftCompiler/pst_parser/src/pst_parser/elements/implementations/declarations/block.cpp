#include "preamble.hpp"

namespace pst {
	ParserRef<Block> Block::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Block>(position);

		if (!assertStmtChoice<Block>(state, state[0].is(Keyword::Block))) return nullptr;

		state.parse(out).all(Keyword::Block, &out->optional_name, &out->code_block);

		return out;
	}

	void Block::dprint(std::ostream& out) const {
		out << R"({"Block": {)";

		out << R"("optional name": )";
		nullAwareDprint(optional_name, out);
		out << R"(, "code block": )";
		nullAwareDprint(code_block, out);
		out << "}}";
	}

	void Block::semPrint(std::ostream& out) const {
		out << R"({"Block": {)";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "namespace")";  // TODO: maybe needs a change?
		out << R"(,"optional name": )";
		nullAwareSemanticTokenPrint(optional_name, out);
		out << R"(, "code block": )";
		nullAwareSemanticTokenPrint(code_block, out);
		out << "}}";
	}

	void Block::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitBlock(*this); }
}
