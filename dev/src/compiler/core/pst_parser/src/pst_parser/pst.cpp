#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::detail {
	void deleteState(LangParserState* ptr) { delete ptr; }

	Box<LangParserState> makeState(tpc::TokenStream&& token_stream, Ref<dia::Logger> logger) {
		return base::makeBox<LangParserState>(std::move(token_stream), logger);
	}

	std::vector<ImportType> extractState(Box<LangParserState> state_ptr) {
		return std::move(*state_ptr).extractState();
	}
}
