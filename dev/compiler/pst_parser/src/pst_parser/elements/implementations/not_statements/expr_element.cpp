#include "preamble.hpp"

#include "../../hierarchy/expr.hpp"

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
			state.log(base::make_unique<EmptyExprError>(state.getPosition()));
			fastForward(state, length);
			return false;
		}
		if (state[length - 1].is(lexer::Token::Type::Sentinel))
			CORE_PANIC("Internal error too long expression\n");
		return true;
	}

	namespace {
		bool universalEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Comma) || state[fwd].is(Special::Semicolon)
			    || ExprClassify::isAssignment(state, fwd);
		}

		bool universalEndAllowComma(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || ExprClassify::isAssignment(state, fwd);
		}
	}

	MBox<ExprElement> UniversalExpr::parse(LangParserState& state) {
		return expr::parseUntil<expr::Ternary, universalEnd>(state);
	}

	MBox<ExprElement> CommaExpr::parse(LangParserState& state) {
		return expr::parseUntil<expr::Comma, universalEndAllowComma>(state);
	}
}
