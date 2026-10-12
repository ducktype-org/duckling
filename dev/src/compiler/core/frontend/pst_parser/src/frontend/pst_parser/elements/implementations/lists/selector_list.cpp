// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/lists/selector_list.hpp"

#include "impl_template.hpp"

DEFAULT_BOX_PTR_DELETER_DEFINITION(pst::NestedSelectorList)

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(SelectorList, elements);

	MBox<SelectorList> SelectorList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			Selector,
			SelectorList,
			true,   // empty list not allowed
			false,  // trailing separator not allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isSemicolonOrSentinel,
			internal::NameGetters::selectorList>(state);
	}

	CLONE_SUB_ELEMENTS_DEF(NestedSelectorList, elements);

	MBox<NestedSelectorList> NestedSelectorList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			Selector,
			NestedSelectorList,
			true,  // empty list not allowed
			true,  // trailing separator allowed
			lexer::Token::BracketType::Curly,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::nestedSelectorList>(state);
	}
}
