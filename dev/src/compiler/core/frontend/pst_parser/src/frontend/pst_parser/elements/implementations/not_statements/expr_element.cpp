// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/not_statements/expr_element.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	bool ExprElement::checkNonEmpty(LangParserState& state) {
		if (state.ctokens().size() == 0) {
			// Empty expression error
			state.logInt(makeBox<EmptyExprError>(state.getPosition()));
			return false;
		}
		return true;
	}

	bool ExprElement::isPrefixOperatorPart(const LangParserState& state, const i64 fwd) {
		if (state[fwd].isPrefixOperator()) return true;
		return fwd > 0 && state[fwd].is(lang_def::Keyword::Mut) && state[fwd - 1].isKeyword()
		    && lang_def::writableViewOperator(state[fwd - 1].asKeyword()).has_value();
	}

	bool ExprElement::isStandaloneKeywordOperator(const LangParserState& state) {
		return state.ctokens().size() == 1 && state[0].isKeyword() && state[0].isPrefixOperator();
	}
}
