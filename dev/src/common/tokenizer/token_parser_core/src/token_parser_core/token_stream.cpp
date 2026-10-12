// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file token_stream.cpp
 */

#include "token_stream.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>

#include <iostream>

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
		  from(from),
		  to(to),
		  sentinel_end(sentinel_end),
		  sentinel_begin(sentinel_begin) {
		CORE_ASSERT(tokens.size() >= to, "TokenStream received too few tokens.");
		CORE_ASSERT(from <= to, "TokenStream received illegal from-to values");
	}

	TokenStream::TokenStream(TokenStream&& stream) noexcept:
		  tokens(stream.tokens),
		  where(stream.where),
		  from(stream.from),
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

	TokenStream TokenStream::getSubstream(u64 length) const {
		CORE_ASSERT(to - where >= length || length == 0, "Sub-stream should fit in parent stream");
		return { tokens,
			     peek(-1).asSentinel(),
			     peek(base::safeIntConv<i64>(length)).asSentinel(),
			     where,
			     where + length };
	}

	const Token& TokenStream::next() { return (where >= to ? sentinel_end : tokens[where++]); }

	const Token& TokenStream::peek(i64 fwd) const {
		if (base::safeIntConv<i64>(where) + fwd < base::safeIntConv<i64>(from))
			return sentinel_begin;
		return (
			base::safeIntConv<i64>(where) + fwd >= base::safeIntConv<i64>(to)
				? sentinel_end
				: tokens[base::safeIntConv<u64>(base::safeIntConv<i64>(where) + fwd)]
		);
	}

	void TokenStream::skip(i64 n) {
		where = base::safeIntConv<usize>(
			std::max(base::safeIntConv<i64>(where) + n, base::safeIntConv<i64>(from))
		);
	}

	usize TokenStream::size() const { return (where >= to ? 0 : to - where); }

}
