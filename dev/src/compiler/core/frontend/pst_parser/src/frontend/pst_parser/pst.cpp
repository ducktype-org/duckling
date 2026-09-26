#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::internal {
	Box<LangParserState> makeState(
		tpc::TokenStream&& token_stream, Box<LangParserContext>&& ctx, Ref<dia::Logger> int_logger
	) {
		return base::makeBox<LangParserState>(std::move(token_stream), std::move(ctx), int_logger);
	}

	void finalizeParsing(Ref<LangParserState> state) { state->finalize(); }
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(pst::LangParserState)
