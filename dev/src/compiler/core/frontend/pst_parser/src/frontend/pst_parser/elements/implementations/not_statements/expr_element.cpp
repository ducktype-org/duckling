#include "../../hierarchy/not_statements/expr_element.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	class EmptyExprError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Empty expression where non-empty expected";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyExprError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	void ExprElement::fastForward(LangParserState& state, i64 length) {
		state.tokens().skip(length);
	}

	bool ExprElement::checkLength(LangParserState& state, i64 length) {
		if (length <= 0) {
			// Empty expression error
			state.log(makeBox<EmptyExprError>(state.getPosition()));
			fastForward(state, length);
			return false;
		}
		if (state[length - 1].is(lexer::Token::Type::Sentinel))
			CORE_PANIC("Internal error too long expression\n");
		return true;
	}
}
