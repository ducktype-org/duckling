#include "parser_state.hpp"

#include <base/except/exceptions.hpp>

namespace tpc {
	TokenStream& ParserState::tokens() { return stream_stack.back(); }

	const TokenStream& ParserState::ctokens() const { return stream_stack.back(); }

	bool ParserState::empty() const { return ctokens().size() == 0; }

	bool ParserState::notEmpty() const { return ctokens().size() > 0; }

	bool ParserState::isSkipping() const {
		return skip_till_fallback;
	}

	bool ParserState::isEOF(i64 fwd) const {
		return stream_stack.size() == 1 && ctokens().size() <= fwd;
	}

	dia::SourcePosition ParserState::getPosition(i64 fwd_from, i64 fwd_to) const {
		fwd_from = std::min(fwd_from, (i64) ctokens().size());
		fwd_to   = std::min(fwd_to, (i64) ctokens().size());
		if (fwd_from >= fwd_to) return getPosition(fwd_from);

		auto base = ctokens().peek(fwd_from).getPosition();

		usize end = ctokens().peek(fwd_to).getPosition().getEnd();
		if (isEOF(fwd_to)) end = ctokens().peek(fwd_to - 1).getPosition().getEnd();
		return { base, end };
	}

	void ParserState::goDown() { 
		stream_stack.emplace_back(tokens().getRecursive()); 
		fallback_types.push_back(SubStreamType::Recursive);
	}

	void ParserState::goUp() {
		CORE_ASSERT(fallback_types.size() && fallback_types.back() == SubStreamType::Recursive, "No recursive token stack to go up from");
		fallback_types.pop_back();
		stream_stack.pop_back();
	}

	void ParserState::goUpAndSkip() {
		goUp();
		tokens().skip();
	}

	void ParserState::goDown() { 
		stream_stack.emplace_back(tokens().getRecursive()); 
		stack_types.push_back(SubStackType::Recursive);
	}

}
