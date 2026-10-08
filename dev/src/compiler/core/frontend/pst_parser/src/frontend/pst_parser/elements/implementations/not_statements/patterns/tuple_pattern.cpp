// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/patterns/tuple_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TuplePattern, elements);

	MBox<TuplePattern> TuplePattern::parse(LangParserState& state) {
		if (!state[0].isBracketGroup(lexer::Token::BracketType::Round)) return nullptr;
		auto out = makeBox<TuplePattern>(state);
		PARSE().one(&out->elements);
		PST_RETURN out;
	}

	void TuplePattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "tuple",)";
		out << R"("elements": [)";
		nullAwareDprint(elements, out);
		out << "]}";
	}

	HashAlg& TuplePattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void TuplePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitTuplePattern(*this);
	}
}
