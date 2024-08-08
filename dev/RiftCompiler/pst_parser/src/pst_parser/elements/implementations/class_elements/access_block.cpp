#include "preamble.hpp"

namespace pst {
	ParserRef<AccessBlock> AccessBlock::parse(RiftParserState& state, tpc::Identifier class_name) {
		auto position = state.getPosition();
		auto out      = makeRef<Block>(position);

		if (!assertStmtChoice<Block>(state, state[0].is(Keyword::Block))) return nullptr;

		state.parse(out).all(Keyword::Block, &out->optional_name, &out->code_block);

		return out;
	}

	void Block::dprint(std::ostream& out) const {
		out << "{";

		out << R"("optional name": )";
		nullAwareDprint(optional_name, out);
		out << R"(, "code block": )";
		nullAwareDprint(code_block, out);

		out << "}";
	}

	void Block::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitBlock(*this); }
}
