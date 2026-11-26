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

		// `mut` is only a part of a writable view (`ref mut T`)
		if (state[0].is(Keyword::Mut)) {
			// `mut ref T` is most likely a swapped `ref mut T` and may be a common mistake, so we
			// log a more specific error.
			if (state[1].isKeyword()
			    && lang_def::writableViewOperator(state[1].asKeyword()).has_value()) {
				state.logSafeError(makeBox<error::MutBeforeViewError>(
					state.getPosition(), lang_def::keywordToStr(state[1].asKeyword()).str()
				));
			} else {
				state.logSafeError(makeBox<error::MisplacedMutError>(state.getPosition()));
			}
			state.tokens().skip();
			// An alone `mut` is already reported. We don't want the "empty where non-empty" error
			// as well.
			if (state.ctokens().size() == 0) return nullptr;
			return parse(state);
		}

		if (!state[0].isPrefixOperator() || isStandaloneKeywordOperator(state))
			return Lower::parse(state);

		auto out = makeBox<GeneralPrefix>(state);

		PARSE().one(&out->op).with(&out->expr, parse);

		PST_RETURN out;
	}
}
