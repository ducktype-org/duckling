// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/lists/template_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TemplateList, elements);

	MBox<TemplateList> TemplateList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolderLowerLevel,
			TemplateList,
			false,  // empty list allowed
			true,   // trailing separator not allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::templateList>(state);
	}
}
