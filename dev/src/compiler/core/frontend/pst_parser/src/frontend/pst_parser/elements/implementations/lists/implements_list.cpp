// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/lists/implements_list.hpp"

#include "../../hierarchy/expressions/ternary.hpp"
#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ImplementsList, elements);

	MBox<ImplementsList> ImplementsList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ImplementsElementExprHolder,
			ImplementsList,
			true,   // empty list not allowed
			false,  // trailing separator not allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isBlockGroup,
			internal::NameGetters::inheritanceList>(state);
	}
}
