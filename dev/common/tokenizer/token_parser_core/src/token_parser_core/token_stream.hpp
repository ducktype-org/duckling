/**
 * @file token_stream.cpp
 */

#pragma once

#include <lexer/token.hpp>

namespace tpc {
	using rift_def::Keyword;
	using lexer::Special;
	using lexer::Token;
	using lexer::Tokens;

	/**
	 * @brief Implements the main ways for a parser to interact with a list of tokens in a safe
	 * way(index wise)
	 */
	class TokenStream {
		const Tokens& tokens;          ///< Source list of tokens
		usize         where = 0;       ///< current position
		usize         to;              ///< end position
		const Token&  sentinel_end;    ///< Token to return if out of bounds forward
		const Token&  sentinel_begin;  ///< Token to return if out of bounds backwards

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

		/**
		 * @brief Returns token at current position then increases the current position
		 */
		const Token& next();

		/**
		 * @brief Returns a token relative to the current position
		 *
		 * @param fwd distance forward from the current position to checked token
		 */
		[[nodiscard]]
		const Token& peek(i64 fwd = 0) const;
		/**
		 * @brief Increases the current position by `n`
		 */
		void skip(usize n = 1);

		/**
		 * @brief Go into the recursive stream of the current token. If the current token doesn't
		 * have a recursive stream throws ``base::LogicError``.
		 *
		 * @return TokenStream used to access the recursive tokens of the current token.
		 */
		[[nodiscard]]
		TokenStream getRecursive() const;

		/**
		 * @brief Calculates the amount of tokens left including the current one
		 */
		[[nodiscard]]
		usize size() const;
	};

}
