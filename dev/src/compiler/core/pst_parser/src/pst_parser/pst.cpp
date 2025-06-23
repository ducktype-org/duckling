#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::detail {
	void StateDeleter::operator()(LangParserState* ptr) { delete ptr; }

	std::unique_ptr<LangParserState, StateDeleter> makeState(
		tpc::TokenStream&& token_stream, Ref<dia::Logger> logger
	) {
		auto* state_ptr = new LangParserState(std::move(token_stream), logger);
		return { state_ptr, StateDeleter() };
	}

	std::vector<ImportType> extractState(std::unique_ptr<LangParserState, StateDeleter> state_ptr) {
		return std::move(*state_ptr).extractState();
	}
}
