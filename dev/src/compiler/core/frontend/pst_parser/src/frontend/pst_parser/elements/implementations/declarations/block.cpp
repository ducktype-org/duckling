#include "../../hierarchy/declarations/block.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Block> Block::parse(LangParserState& state) {
		auto out = makeBox<Block>(state);

		if (!assertStmtChoice<Block>(state, state[0].is(Keyword::Block))) return nullptr;

		PARSE().all(Keyword::Block, &out->optional_name, &out->code_block);

		PST_RETURN out;
	}

	void Block::dprint(std::ostream& out) const {
		out << "{";

		out << R"("optional name": )";
		nullAwareDprint(optional_name, out);
		out << R"(, "code block": )";
		nullAwareDprint(code_block, out);

		out << "}";
	}

	HashAlg& Block::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, optional_name);
		return partial_hash;
	}

	void Block::acceptVisitor(PstVisitor& visitor) const { visitor.visitBlock(*this); }
}
