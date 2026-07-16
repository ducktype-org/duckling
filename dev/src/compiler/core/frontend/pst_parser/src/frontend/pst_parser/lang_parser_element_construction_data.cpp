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
