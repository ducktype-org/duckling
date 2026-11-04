// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/declarations/block.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Block, name, code_block);

	MBox<Block> Block::parse(LangParserState& state) {
		auto out = makeBox<Block>(state);

		if (!assertStmtChoice<Block>(state, state[0].is(Keyword::Block))) return nullptr;

		PARSE().all(Keyword::Block, &out->name, &out->code_block);

		PST_RETURN out;
	}

	void Block::dprint(std::ostream& out) const {
		out << "{";

		if (name.has_value()) {
			out << R"("name": )";
			nullAwareDprint(name.value(), out);
			out << ",";
		}
		out << R"("code block": )";
		nullAwareDprint(code_block, out);

		out << "}";
	}

	HashAlg& Block::addElementDataToStableHash(HashAlg& partial_hash) const { return partial_hash; }

	void Block::acceptVisitor(PstVisitor& visitor) const { visitor.visitBlock(*this); }
}
