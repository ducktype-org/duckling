#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::internal {
	Box<LangParserState> makeState(tpc::TokenStream&& token_stream, Ref<dia::Logger> logger) {
		return base::makeBox<LangParserState>(std::move(token_stream), logger);
	}

	std::vector<ImportType> extractState(Box<LangParserState> state_ptr) {
		return std::move(*state_ptr).extractState();
	}
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(pst::LangParserState)
