// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/statements/expand.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Expand, value);

	MBox<Expand> Expand::parse(LangParserState& state) {
		auto out = makeBox<Expand>(state, state.getContext());

		if (!assertStmtChoice<Expand>(state, state[0].is(Keyword::Expand))) return nullptr;

		PARSE().all(Keyword::Expand, &out->value);

		PST_RETURN out;
	}

	/**
	 * @note For now this will break on escape characters.
	 */
	void Expand::dprint(std::ostream& out) const {
		out << "{";

		out << R"("value": ")";
		nullAwareDprint(value, out);
		out << R"(",)";

		out << "}";
	}

	HashAlg& Expand::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Expand::acceptVisitor(PstVisitor& visitor) const { visitor.visitExpand(*this); }
}
