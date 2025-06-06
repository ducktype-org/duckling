/**
 * @file token_stream.cpp
 */

#include "token_stream.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>

#include <iostream>
#include <utility>

namespace tpc {

	TokenStream::TokenStream(
		const Tokens& tokens,
		const Token&  sentinel_begin,
		const Token&  sentinel_end,
		usize         from,
		usize         to
	):
		  tokens(tokens),
		  where(from),
		  to(to),
		  sentinel_end(sentinel_end),
		  sentinel_begin(sentinel_begin) {
		CORE_ASSERT(tokens.size() >= to, "TokenStream received too few tokens.");
		CORE_ASSERT(from <= to, "TokenStream received illegal from-to values");
	}

	TokenStream::TokenStream(TokenStream&& stream) noexcept:
		  tokens(stream.tokens),
		  where(stream.where),
		  to(stream.to),
		  sentinel_end(stream.sentinel_end),
		  sentinel_begin(stream.sentinel_begin) {}

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
		if (std::max(-fwd, (i64) 0) > where) return sentinel_begin;
		return (
			base::safeIntConv<i64>(where) + fwd >= to ? sentinel_end : tokens[where + (usize) fwd]
		);
	}

	void TokenStream::skip(i64 n) {
		where = base::safeIntConv<usize>(std::max(base::safeIntConv<i64>(where) + n, (i64) 0));
	}

	usize TokenStream::size() const { return (where >= to ? 0 : to - where); }

}
