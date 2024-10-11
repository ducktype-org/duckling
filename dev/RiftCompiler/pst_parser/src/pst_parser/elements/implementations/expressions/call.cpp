#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Call::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Call Expression" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1
		        && (state[0].isBracketGroup(lexer::Token::Round)
		            || state[0].isBracketGroup(lexer::Token::Square)))) {}  // Error

		auto out = base::make_unique<Call>(state.getPosition());

		state.parse(out).goDown();
		state.parse(out).with(&out->args, Comma::parse, state.ctokens().size());
		state.parse(out).goUpAndSkip();

		return out;
	}
}
