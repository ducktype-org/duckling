#include "../../hierarchy/not_statements/expr_element.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {

	void ExprElement::fastForward(LangParserState& state, i64 length) {
		state.tokens().skip(length);
	}

	bool ExprElement::checkLength(LangParserState& state, i64 length) {
		if (length <= 0) {
			// Empty expression error
			state.logInt(makeBox<EmptyExprError>(state.getPosition()));
			fastForward(state, length);
			return false;
		}
		if (state[length - 1].is(lexer::Token::Type::Sentinel))
			CORE_PANIC("Internal error too long expression\n");
		return true;
	}

	bool ExprElement::checkNonEmpty(LangParserState& state) {
		if (state.ctokens().size() == 0) {
			// Empty expression error
			state.logInt(makeBox<EmptyExprError>(state.getPosition()));
			return false;
		}
		return true;
	}
}
