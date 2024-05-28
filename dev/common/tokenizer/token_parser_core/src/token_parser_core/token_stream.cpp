/**
 * @file token_stream.cpp
 */

#include <base/exceptions.hpp>
#include <utility>
#include <iostream>
#include "token_stream.hpp"

namespace tpc {

	TokenStream::TokenStream(const Tokens& tokens, Token sentinel_begin, Token sentinel_end, usize from, usize to):
		  tokens(tokens),
		  where(from),
		  to(to),
		  sentinel_end(std::move(sentinel_end)),
		  sentinel_begin(std::move(sentinel_begin)) {
		RIFT_ASSERT(tokens.size() >= to, "TokenStream received too few tokens.");
		RIFT_ASSERT(from <= to, "TokenStream received illegal from-to values");
	}

	TokenStream::TokenStream(TokenStream&& stream) noexcept:
		  tokens(stream.tokens),
		  where(stream.where),
		  to(stream.to),
		  sentinel_end(std::move(stream.sentinel_end)),
		  sentinel_begin(std::move(stream.sentinel_begin)) {}

	TokenStream TokenStream::getRecursive() const {
		if (peek().isRecursive()) {
			const auto& rec = peek().getRecursive();
			return { rec, peek().getSentinelBegin(), peek().getSentinelEnd(), 0, rec.size() };
		} else {
			// @TODO
			throw base::LogicError("get recursive on non-recursive token");
		}
	}

	const Token& TokenStream::next() { return (where >= to ? sentinel_end : tokens[where++]); }

	const Token& TokenStream::peek(i64 fwd) const {
		if (std::max(-fwd, (i64)0) > where) return sentinel_begin;
		return (where + fwd >= to ? sentinel_end : tokens[where + fwd]);
	}

	void TokenStream::skip(usize n) { where += n; }

	bool TokenStream::isKeyword(i64 fwd) const { return peek(fwd).isKeyword(); }

	Keyword TokenStream::asKeyword(i64 fwd) const { return peek(fwd).asKeyword(); }

	bool TokenStream::isSpecial(i64 fwd) const { return peek(fwd).isSpecial(); }

	Special TokenStream::asSpecial(i64 fwd) const { return peek(fwd).asSpecial(); }

	bool TokenStream::isOperator(i64 fwd) const { return peek(fwd).isOperator(); }

	bool TokenStream::isOperator(base::StrId oper, i64 fwd) const {
		return peek(fwd).isOperator() and peek(fwd).isStr(oper);
	}

	bool TokenStream::isBracketGroup(i64 fwd) const { return peek(fwd).isBracketGroup(); }

	bool TokenStream::isBracketGroup(Token::BracketType bracket_type, i64 fwd) const {
		return peek(fwd).isBracketGroup(bracket_type);
	}

	bool TokenStream::isRecursive(i64 fwd) const { return peek(fwd).isRecursive(); }

	usize TokenStream::size() const { return (where >= to ? 0 : to - where); }

}
