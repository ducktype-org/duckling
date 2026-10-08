// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lang_parser_element_construction_data.hpp"

#include "lang_parser_state.hpp"

namespace pst {
	LangParserElementConstructionData::LangParserElementConstructionData(const LangParserState& state
	):
		  source_position(state.getPosition()) {
		HashAlg partial_hash;
		addToHash(partial_hash, *state.getContext());
		context_hash = partial_hash.finalize();
	}
}
