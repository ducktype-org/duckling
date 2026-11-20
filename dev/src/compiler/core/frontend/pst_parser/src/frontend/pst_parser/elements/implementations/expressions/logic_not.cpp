// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/logic_not.hpp"

#include "../../hierarchy/expressions/comparison_chain.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicNot::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;


		if (!state[0].is(Keyword::Not)) return Lower::parse(state);

		auto out = makeBox<LogicNot>(state);

		PARSE().one(&out->op).with(&out->expr, Self::parse);

		PST_RETURN out;
	}
}
