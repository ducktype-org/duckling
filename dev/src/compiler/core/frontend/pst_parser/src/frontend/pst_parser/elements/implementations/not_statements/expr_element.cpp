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
}
