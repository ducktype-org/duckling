/**
 * @file token_stream.cpp
 */

#pragma once

#include <lexer/token.hpp>
#include <base/string_id.hpp>
#include "error_state.hpp"

namespace tpc {
	using lexer::Keyword;
	using lexer::Special;
	using lexer::Token;
	using lexer::Operator;

	class TokenStream {
		const Tokens& tokens;
		usize where = 0;
		/** exclusive */
		usize to;

		// sentinel_begin is not necessary right now.
		// In will be necessary if TokenStream will allow to move backward.
		lexer::Token sentinel_end;

	public:
		TokenStream() = delete;
		TokenStream(TokenStream&) = delete;
		TokenStream(TokenStream&&) noexcept;

		TokenStream(const Tokens& tokens, Token&& sentinel_end, usize from, usize to);

		const Token& next();

		TokenStream getRecursive() const;

		[[nodiscard]]
		const Token& peek(usize fwd = 0) const;
		void skip(usize n = 1);

		[[nodiscard]]
		bool isKeyword(usize fwd = 0) const;
		[[nodiscard]]
		Keyword asKeyword(usize fwd = 0) const;

		[[nodiscard]]
		bool isSpecial(usize fwd = 0) const;
		[[nodiscard]]
		Special asSpecial(usize fwd = 0) const;

		[[nodiscard]]
		bool isOperator(usize fwd = 0) const;
		[[nodiscard]]
		bool isOperator(base::StrId oper, usize fwd = 0) const;

		template<class T>
		[[nodiscard]]
		bool is(T t, usize fwd = 0) const {
			return peek(fwd).is(t);
		}

		[[nodiscard]]
		usize size() const;

	};

}
