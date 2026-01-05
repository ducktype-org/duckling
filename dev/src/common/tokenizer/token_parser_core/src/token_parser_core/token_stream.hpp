/**
 * @file token_stream.cpp
 */

#pragma once

#include <lexer/token.hpp>

namespace tpc {
	using lang_def::Keyword;
	using lexer::Special;
	using lexer::Token;
	using lexer::Tokens;

	class TokenStream;
	using TokenStreamCondition = bool(const TokenStream&, i64);

	/**
	 * @brief Implements the main ways for a parser to interact with a list of tokens in a safe
	 * way(index wise)
	 */
	class TokenStream {
		const Tokens& tokens;          ///< Source list of tokens
		usize         where;           ///< current position
		usize         from, to;        ///< begin and end position.
		const Token   sentinel_end;    ///< Token to return if out of bounds forward
		const Token   sentinel_begin;  ///< Token to return if out of bounds backwards

	public:
		TokenStream()             = delete;
		TokenStream(TokenStream&) = delete;
		TokenStream(TokenStream&&) noexcept;

		TokenStream(
			const Tokens& tokens,
			const Token&  sentinel_begin,
			const Token&  sentinel_end,
			usize         from,
			usize         to
		);

		[[nodiscard]]
		const Token& operator[](i64 fwd) const {
			return peek(fwd);
		}

		/**
		 * @brief Returns token at current position then increases the current position
		 */
		const Token& next();

		/**
		 * @brief Returns a token relative to the current position
		 *
		 * @param fwd distance forward from the current position to checked token. negative value
		 * indicates backward position.
		 */
		[[nodiscard]]
		const Token& peek(i64 fwd = 0) const;
		/**
		 * @brief Increases the current position by `n`
		 */
		void skip(i64 n = 1);

		/**
		 * @brief Go into the recursive stream of the current token. If the current token doesn't
		 * have a recursive stream throws ``base::LogicError``.
		 *
		 * @return TokenStream used to access the recursive tokens of the current token.
		 */
		[[nodiscard]]
		TokenStream getRecursive() const;

		/**
		 * @brief Return a sub stream starting from this token of given length. Panics on too long
		 * of a length.
		 */
		[[nodiscard]]
		TokenStream getSubstream(u64 length) const;

		template<TokenStreamCondition until>
		[[nodiscard]]
		u64 countUntil(i64 base = 0) const {
			u64 length = 0;
			while (!peek(base + base::safeIntConv<i64>(length)).is(Token::Type::Sentinel)
			       && !until(*this, base + base::safeIntConv<i64>(length))) {
				length++;
			}
			return length;
		}

		/**
		 * @brief Calculates the amount of tokens left including the current one
		 */
		[[nodiscard]]
		usize size() const;
	};
}
