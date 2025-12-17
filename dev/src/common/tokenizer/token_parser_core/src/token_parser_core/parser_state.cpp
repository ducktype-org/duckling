#include "parser_state.hpp"

#include <base/except/exceptions.hpp>

namespace tpc {
	TokenStream& ParserState::tokens() { return *current_stream; }

	const TokenStream& ParserState::ctokens() const { return *current_stream; }

	bool ParserState::empty() const { return ctokens().size() == 0; }

	bool ParserState::notEmpty() const { return ctokens().size() > 0; }

	bool ParserState::isSkipping() const { return skip_till_fallback; }

	bool ParserState::isEOF(i64 fwd) const {
		return fallback_stack.empty() && ctokens().size() <= fwd;
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
		auto new_stream = ctokens().getRecursive();
		fallback_stack.emplace_back(Fallback{
			.type = Recursive, .saved_stream = std::move(*current_stream), .post_jump = 1 });
		current_stream = makeBox<TokenStream>(std::move(new_stream));
	}

	void ParserState::goUp() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::Recursive,
			"No recursive token stream to go up from"
		);
		checkAllParsed();
		current_stream = makeBox<TokenStream>(std::move(fallback_stack.back().saved_stream));
		fallback_stack.pop_back();
		skip_till_fallback = false;
	}

	void ParserState::goUpAndSkip() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::Recursive,
			"No recursive token stream to go up from"
		);
		checkAllParsed();
		u64 fwd = fallback_stack.back().post_jump;
		current_stream = makeBox<TokenStream>(std::move(fallback_stack.back().saved_stream));
		fallback_stack.pop_back();
		tokens().skip(base::safeIntConv<i64>(fwd));
		skip_till_fallback = false;
	}

	void ParserState::setFallback(u64 length) {
		auto new_stream = ctokens().getSubstream(length);
		fallback_stack.emplace_back(Fallback{ .type         = NonRecursive,
		                                      .saved_stream = std::move(*current_stream),
		                                      .post_jump    = length });
		current_stream = makeBox<TokenStream>(std::move(new_stream));
	}

	void ParserState::exitFallback() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::NonRecursive,
			"No fallback token stream to go up from"
		);
		checkAllParsed();
		u64 fwd = fallback_stack.back().post_jump;
		current_stream = makeBox<TokenStream>(std::move(fallback_stack.back().saved_stream));
		fallback_stack.pop_back();
		tokens().skip(base::safeIntConv<i64>(fwd));
		skip_till_fallback = false;
	}

	void ParserState::finalize() {
		finalized = true;
		checkAllParsed();
	}

	bool ParserState::isFinalized() const {
		return finalized;
	}

	class NotAllParsedError;
	void ParserState::checkAllParsed() {
		if (!isSkipping() && !empty()) {
			log(base::makeBox<NotAllParsedError>(dia::SourcePosition{
				getPosition(), 
				ctokens()[base::safeIntConv<i64>(ctokens().size()) - 1].getPosition().getEnd()
			}));
		}
	}

	class NotAllParsedError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected additional tokens during parsing.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NotAllParsedError(dia::SourcePosition pos): dia::Error(pos) {}
	};
}
