// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/general_prefix.hpp"

#include "../../hierarchy/expressions/chain_expr.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> GeneralPrefix::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (!state[0].isPrefixOperator()) return Lower::parse(state);

		auto out = makeBox<GeneralPrefix>(state);

		PARSE().one(&out->op).with(&out->expr, parse);

		PST_RETURN out;
	}
}
