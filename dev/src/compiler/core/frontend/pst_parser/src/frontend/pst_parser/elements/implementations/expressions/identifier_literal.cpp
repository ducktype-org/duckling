// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/identifier_literal.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(IdentifierLiteral, name);

	MBox<ExprElement> IdentifierLiteral::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<IdentifierLiteral>(state);

		PARSE().one(&out->name);

		PST_RETURN out;
	}

	void IdentifierLiteral::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);

		out << "}";
	}

	HashAlg& IdentifierLiteral::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void IdentifierLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitIdentifierLiteral(*this);
	}
}
