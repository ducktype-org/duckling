// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/logic_or.hpp"

#include "../../hierarchy/expressions/logic_and.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicOr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		bool or_found = false;
		i64  or_fwd   = 0;

		for (i64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::Or)) {
				or_found = true;
				or_fwd   = i;
				break;
			}
		}
		if (!or_found) return Lower::parse(state);

		auto out = makeBox<LogicOr>(state);

		PARSE().autoFallbackLen(or_fwd).with(&out->left, Lower::parse);
		PARSE().one(&out->op);
		PARSE().with(&out->right, Self::parse);

		PST_RETURN out;
	}
}
