// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/lists/array_literal_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ArrayLiteralList, elements);

	MBox<ArrayLiteralList> ArrayLiteralList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			ArrayLiteralList,
			false,  // empty list allowed
			true,   // trailing separator allowed
			lexer::Token::BracketType::Square,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::arrayLiteralList>(state);
	}
}
