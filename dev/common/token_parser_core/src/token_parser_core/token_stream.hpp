/**
 * @file token_stream.cpp
 */

#pragma once

#include "error_state.hpp"
#include <base/string_id.hpp>
#include <lexer/token.hpp>

namespace tpc {
	using lexer::Keyword;
	using lexer::Operator;
	using lexer::Special;
	using lexer::Token;

	void tokenStreamInit();

	class TokenStream {
		const Tokens &tokens;
		usize         where = 0;
		/** inclusive */
		usize               to;
		static lexer::Token sentinel;
		friend void         tokenStreamInit();

	public:
		TokenStream()              = delete;
		TokenStream(TokenStream &) = delete;
		TokenStream(TokenStream &&) noexcept;

		TokenStream(const Tokens &tokens, usize from, usize to):
			  tokens(tokens),
			  where(from),
			  to(to) {}

		const Token &next();

		TokenStream getRecursive() const;

		[[nodiscard]]
		const Token &peek(usize fwd = 0) const;
		void         skip(usize n = 1);

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
