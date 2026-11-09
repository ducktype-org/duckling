// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lang_element_construction_data.hpp"

#include "../state/lang_parser_state.hpp"

namespace pst {
	LangElementConstructionData::LangElementConstructionData(const LangParserState& state):
		  context_hash(state.getContextHash()),
		  source_position(state.getPosition()) {}
}
