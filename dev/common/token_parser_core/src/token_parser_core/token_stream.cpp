/**
 * @file token_stream.cpp
 */

#include <base/exceptions.hpp>
#include "token_stream.hpp"

namespace tpc {

	TokenStream::TokenStream(const Tokens& tokens, const Token& sentinel_end, usize from, usize to):
		  tokens(tokens),
		  where(from),
		  to(to),
		  sentinel_end(sentinel_end) {
		RIFT_ASSERT(tokens.size() >= to, "TokenStream received too few tokens.");
		RIFT_ASSERT(from <= to, "TokenStream received illegal from-to values");
	}

	TokenStream::TokenStream(TokenStream&& stream) noexcept:
		  tokens(stream.tokens),
		  where(stream.where),
		  to(stream.to),
		  sentinel_end(std::move(stream.sentinel_end)) {}

	TokenStream TokenStream::getRecursive() const {
		if (peek().isGroup()) {
			const auto& rec = peek().getRecursive();
			return TokenStream(rec, peek().getSentinel(), 0, rec.size());
		} else {
			// @TODO
			throw base::LogicError("get recursive on no group");
		}
	}

	const Token& TokenStream::next() { return (where >= to ? sentinel_end : tokens[where++]); }

	const Token& TokenStream::peek(usize fwd) const {
		return (where + fwd >= to ? sentinel_end : tokens[where + fwd]);
	}

	void TokenStream::skip(usize n) { where += n; }

	bool TokenStream::isKeyword(usize fwd) const { return peek(fwd).isKeyword(); }

	Keyword TokenStream::asKeyword(usize fwd) const { return peek(fwd).asKeyword(); }

	bool TokenStream::isSpecial(usize fwd) const { return peek(fwd).isSpecial(); }

	Special TokenStream::asSpecial(usize fwd) const { return peek(fwd).asSpecial(); }

	bool TokenStream::isOperator(usize fwd) const { return peek(fwd).isOperator(); }

	bool TokenStream::isOperator(base::StrId oper, usize fwd) const {
		return peek(fwd).isOperator() and peek(fwd).isStr(oper);
	}

	bool TokenStream::isGroup(usize fwd) const {
		return peek(fwd).isGroup();
	}

	bool TokenStream::isGroup(UChar32 group_type, usize fwd) const {
		return peek(fwd).isGroup(group_type);
	}


	usize TokenStream::size() const { return (where >= to ? 0 : to - where); }

}
