// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/keyword_literal.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(KeywordLiteral, keyword);

	MBox<ExprElement> KeywordLiteral::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<KeywordLiteral>(state);

		PARSE().one(&out->keyword);

		PST_RETURN out;
	}

	void KeywordLiteral::dprint(std::ostream& out) const {
		out << "{";

		out << R"("keyword": )";
		nullAwareDprint(keyword, out);

		out << "}";
	}

	HashAlg& KeywordLiteral::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void KeywordLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitKeywordLiteral(*this);
	}
}
