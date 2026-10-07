// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/lists/attribute_arg_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(AtrArgList, elements);

	MBox<AtrArgList> AtrArgList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			AtrArgList,
			false,  // empty list allowed
			true,   // trailing separator allowed
			lexer::Token::BracketType::Round,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::attributeArgList>(state);
	}
}
