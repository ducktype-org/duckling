#include "parser_state.hpp"

namespace tpc {
	TokenStream& ParserState::tokens() { return stream_stack.back(); }

	const TokenStream& ParserState::ctokens() const { return stream_stack.back(); }

	bool ParserState::empty() const { return ctokens().size() == 0; }

	bool ParserState::notEmpty() const { return ctokens().size() > 0; }

	void ParserState::goDown() { stream_stack.emplace_back(std::move(tokens().getRecursive())); }

	void ParserState::goUp() { stream_stack.pop_back(); }

	void ParserState::goUpAndSkip() {
		goUp();
		tokens().skip();
	}

}  // namespace tpc
