// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/class_elements/class_special.hpp"

#include "../../hierarchy/class_elements/constructor.hpp"
#include "../../hierarchy/class_elements/copy_constructor.hpp"
#include "../../hierarchy/class_elements/destructor.hpp"
#include "../../hierarchy/class_elements/move_constructor.hpp"
#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<ClassSpecial> ClassSpecial::parse(LangParserState& state) {
		if (state[1].isBracketGroup(Token::Round)) return Constructor::parse(state);

		if (state[2].is(Keyword::Destroy)) return Destructor::parse(state);
		if (state[2].is(Keyword::Copy)) return CopyConstructor::parse(state);
		if (state[2].is(Keyword::Move)) return MoveConstructor::parse(state);

		return Constructor::parse(state);
	}

	HashAlg& ClassSpecial::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
