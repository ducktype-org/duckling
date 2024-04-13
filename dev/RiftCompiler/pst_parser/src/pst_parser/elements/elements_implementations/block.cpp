#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Block> Block::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Block>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Block), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Block, &out->optional_name, &out->code_block);

		return out;
	}

	void Block::dprint(std::ostream& out) const {
		out << "{\"Block\": {";

		out << R"("optional name": )";
		nullAwareDprint(optional_name, out);
		out << R"(, "code block": )";
		nullAwareDprint(code_block, out);
		out << "}}";
	}
}
