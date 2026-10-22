// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/array_literal.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(ArrayLiteral, list);

	MBox<ExprElement> ArrayLiteral::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<ArrayLiteral>(state);
		PARSE().one(&out->list);

		PST_RETURN out;
	}

	void ArrayLiteral::dprint(std::ostream& out) const {
		out << "{";

		out << R"("list": )";
		nullAwareDprint(list, out);

		out << "}";
	}

	HashAlg& ArrayLiteral::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ArrayLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitArrayLiteral(*this);
	}
}
