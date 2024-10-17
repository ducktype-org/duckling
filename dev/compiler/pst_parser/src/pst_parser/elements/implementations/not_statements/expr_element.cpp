#include "preamble.hpp"

#include "../../hierarchy/expr.hpp"

namespace pst {
	void ExprElement::fastForward(LangParserState& state, i64 length) {
		state.tokens().skip(length);
	}

	bool ExprElement::checkLength(LangParserState& state, i64 length) {
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

	namespace {
		bool universalEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Comma) 
				|| state[fwd].is(Special::Semicolon)
				|| state[fwd].is(NamedOperator::Assign);
		}
		
		bool universalEndAllowComma(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon)
				|| state[fwd].is(NamedOperator::Assign);
		}
	}

	base::unique_ptr<ExprElement> UniversalExpr::parse(LangParserState &state) {
		return expr::parseUntil<expr::Ternary, universalEnd>(state);
	}

	base::unique_ptr<ExprElement> CommaExpr::parse(LangParserState &state) {
		return expr::parseUntil<expr::Comma, universalEndAllowComma>(state);
	}
}
