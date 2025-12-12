#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::internal {
	Box<LangParserState> makeState(
		tpc::TokenStream&& token_stream, Ref<dia::Logger> logger, Ref<dia_int::Logger> int_logger
	) {
		return base::makeBox<LangParserState>(std::move(token_stream), logger, int_logger);
	}

	std::vector<ImportType> extractState(Box<LangParserState> state_ptr) {
		return std::move(*state_ptr).extractState();
	}

	void finalizeParsing(Ref<LangParserState> state) {
		state->finalize();
	}
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(pst::LangParserState)
