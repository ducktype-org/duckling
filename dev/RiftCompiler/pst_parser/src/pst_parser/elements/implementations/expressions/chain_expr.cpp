#include "preamble.hpp"

namespace pst::expr {
	u64 ChainExpr::toNextLink(const RiftParserState& state, u64 length) {
		RIFT_ASSERT(length > 0, "Illegal max length to next link");

		u64 fwd = 1;
		if (state[0].is(Keyword::Lambda)) fwd = 2;  // Skip ()
		while (fwd < length) {
			if (state[fwd].is(rift_def::Operator::Period)) break;
			if (state[fwd].isBracketGroup(lexer::Token::Square)) break;
			if (state[fwd].isBracketGroup(lexer::Token::Round)) break;
			fwd++;
		}

		return fwd;
	}

	ParserRef<ExprElement> ChainExpr::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Chain Expression" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		u64 fwd = toNextLink(state, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = base::make_unique<ChainExpr>(state.getPosition());

		state.parse(out).with(&out->literal, Lower::parse, +fwd);
		length -= fwd;

		while (length > 0) {
			fwd = toNextLink(state, length);
			out->chain.push_back(nullptr);
			if (state[0].is(rift_def::Operator::Period)) {
				state.parse(out).with(&out->chain.back(), Access::parse, +fwd);
			} else if (state[0].isBracketGroup(lexer::Token::Round) || state[0].isBracketGroup(lexer::Token::Square)) {
				state.parse(out).with(&out->chain.back(), Call::parse, +fwd);
			} else {
				// Error
				fastForward(state, fwd);
			}
			length -= fwd;
		}

		return out;
	}
}
