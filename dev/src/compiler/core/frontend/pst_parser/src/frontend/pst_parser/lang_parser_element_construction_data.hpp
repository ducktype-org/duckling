// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "pst_config.hpp"

#include <diagnostic/source_position.hpp>

namespace pst {
	class LangParserState;

	class LangParserElementConstructionData {
	public:
		HashType            context_hash;
		dia::SourcePosition source_position;

		LangParserElementConstructionData(const LangParserState& state);

		LangParserElementConstructionData(HashType context_hash, dia::SourcePosition source_position):
			  context_hash(context_hash),
			  source_position(source_position) {}
	};
}
