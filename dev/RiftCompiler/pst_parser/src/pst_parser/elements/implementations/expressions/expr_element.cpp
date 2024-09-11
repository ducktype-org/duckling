#include "preamble.hpp"

namespace pst {
	void ExprElement::fastForward(RiftParserState& state, i64 length) {
		state.tokens().skip(length);
	}

	bool ExprElement::checkLength(RiftParserState& state, i64 length) {
		if (length == 0) {
			std::cerr << "empty expression" << std::endl;
			// Empty expression error
			fastForward(state, length);
			return false;
		}
		if (state[length - 1].is(lexer::Token::Type::Sentinel)) {
			std::cerr << "too long expression" << std::endl;
			// Expression length too long error
			fastForward(state, length);
			return false;
		}
		return true;
	}
}
