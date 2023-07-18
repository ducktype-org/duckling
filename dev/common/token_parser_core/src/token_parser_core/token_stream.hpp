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

	void tokenStreamInit();

	class TokenStream {
		const Tokens& tokens;
		std::size_t where = 0;
		/** inclusive */
		std::size_t to;
		static lexer::Token sentinel;
		friend void tokenStreamInit();
	public:
		TokenStream() = delete;
		TokenStream(TokenStream&) = delete;
		TokenStream(TokenStream&&) noexcept;

		TokenStream(const Tokens& tokens, std::size_t from, std::size_t to): 
			tokens(tokens), where(from), to(to) {}
		const Token& next();

		TokenStream getRecursive() const;

		[[nodiscard]]
		const Token& peek(std::size_t fwd = 0) const;
		void skip(std::size_t n = 1);

		[[nodiscard]]
		bool isKeyword(std::size_t fwd = 0) const;
		[[nodiscard]]
		Keyword asKeyword(std::size_t fwd = 0) const;

		[[nodiscard]]
		bool isSpecial(std::size_t fwd = 0) const;
		[[nodiscard]]
		Special asSpecial(std::size_t fwd = 0) const;

		[[nodiscard]]
		bool isOperator(std::size_t fwd = 0) const;
		[[nodiscard]]
		bool isOperator(base::StrId oper, std::size_t fwd = 0) const;

		template<class T>
		[[nodiscard]]
		bool is(T t, size_t fwd = 0) const {
			return peek(fwd).is(t);
		}

		[[nodiscard]]
		std::size_t size() const;

	};

}
