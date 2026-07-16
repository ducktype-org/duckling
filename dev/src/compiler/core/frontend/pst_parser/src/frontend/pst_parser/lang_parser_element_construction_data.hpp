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
