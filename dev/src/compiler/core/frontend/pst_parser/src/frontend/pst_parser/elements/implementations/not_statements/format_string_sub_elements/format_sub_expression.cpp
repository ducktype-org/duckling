// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/format_string_sub_elements/format_sub_expression.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(FormatSubExpression, expr);

	MBox<FormatSubExpression> FormatSubExpression::parse(LangParserState& state) {
		auto out = makeBox<FormatSubExpression>(state);

		PARSE().goDown();
		PARSE().one(&out->expr);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void FormatSubExpression::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	HashAlg& FormatSubExpression::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
