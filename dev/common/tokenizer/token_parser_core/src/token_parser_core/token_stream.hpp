/**
 * @file token_stream.cpp
 */

#pragma once

#include <lexer/token.hpp>
#include <base/string_id.hpp>

namespace tpc {
	using lexer::Keyword;
	using lexer::Operator;
	using lexer::Special;
	using lexer::Token;
	using lexer::Tokens;

	/**
	 * @brief Implements the main ways for a parser to interact with a list of tokens in a safe way(index wise)
	 * 
	 * @note We might need to add sentinel_begin if we implement going backwards
	 */
	class TokenStream {
		const Tokens& tokens; ///< Source list of tokens
		usize         where = 0; ///< current position
		usize to; ///< end position
		lexer::Token sentinel_end; ///< Token to return if out of bounds

	public:
		TokenStream()             = delete;
		TokenStream(TokenStream&) = delete;
		TokenStream(TokenStream&&) noexcept;

		TokenStream(const Tokens& tokens, const Token& sentinel_end, usize from, usize to);

		/**
		 * @brief Returns token at current position then increases the current position
		 */
		const Token& next();

		/**
		 * @brief Go into the recursive stream of the current token. If the current token doesn't have a recursive stream throws ``base::LogicError``.
		 * 
		 * @return TokenStream used to access the recursive tokens of the current token.
		 */
		TokenStream getRecursive() const;

		/**
		 * @brief Returns a token relative to the current position
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		const Token& peek(usize fwd = 0) const;
		/**
		 * @brief Increases the current position by `n`
		 */
		void         skip(usize n = 1);

		/**
		 * @brief Checks whether a token is a keyword
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isKeyword(usize fwd = 0) const;
		[[nodiscard]]
		Keyword asKeyword(usize fwd = 0) const;

		/**
		 * @brief Checks whether a token is a special
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isSpecial(usize fwd = 0) const;
		[[nodiscard]]
		Special asSpecial(usize fwd = 0) const;

		/**
		 * @brief Checks whether a token is an operator
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isOperator(usize fwd = 0) const;
		/**
		 * @brief Checks whether a token is a particular operator
		 * 
		 * @param oper string with the chosen operator
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isOperator(base::StrId oper, usize fwd = 0) const;

		/**
		 * @brief Checks whether a token is a bracket group
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isBracketGroup(usize fwd = 0) const;
		/**
		 * @brief Checks whether a token is a particular bracket group
		 * 
		 * @param type chosen bracket group type
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isBracketGroup(Token::BracketType type, usize fwd = 0) const;

		/**
		 * @brief Checks whether a token is recursive
		 * 
		 * @param fwd distance forward from the current position
		 */
		[[nodiscard]]
		bool isRecursive(usize fwd = 0) const;

		/**
		 * @brief Allows for comparing a token with values of multiple different types
		 * 
		 * The possible types are:
		 *  - `lexer::Token::Type`
		 *  - `rift_def::Keyword`
		 *  - `rift_def::Special`
		 *  - `rift_def::Operator`
		 * 
		 * @param fwd distance forward from the current position
		 */
		template<class T>
		[[nodiscard]]
		bool is(T t, usize fwd = 0) const {
			return peek(fwd).is(t);
		}

		/**
		 * @brief Calculates the amount of tokens left including the current one
		 */
		[[nodiscard]]
		usize size() const;
	};

}
