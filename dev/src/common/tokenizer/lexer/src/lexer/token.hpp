// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include "token_common.hpp"

#include <base/misc/raw_view.hpp>

#include <diagnostic/source_position.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace lexer {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;

	class Token;
	/**
	 * @brief Type representing a list of tokens
	 */
	using Tokens = std::vector<lexer::Token>;

	/**
	 * @brief Class representing a single token and providing methods of accessing information about
	 * it
	 */
	class Token final {
	public:
		enum class Type {
			Keyword,
			Identifier,
			NumLiteral,
			NumLiteralGroup,  ///< group storing a numeric literal token and the type specifier token
			TypeSpecifier,
			String,
			Char,
			FormatString,  ///< group
			FormatStringSubString,
			BracketGroup,  ///< group storing opening bracket value in group_type
			Operator,
			Comment,
			Special,
			Empty,
			Sentinel,
			Error
		};

		static std::string typeToStr(Type type);

		/**
		 * @brief Non-exhaustive enum of bracket types
		 *
		 */
		enum BracketType : UChar32 {
			None   = 0,
			Round  = '(',
			Square = '[',
			Curly  = '{',  ///< Is also used for format string sub expressions
			Angle  = 0x30'08,
		};

		/**
		 * @name Functions that construct a Token
		 * @{
		 */
		static Token makeSentinel(base::RawView view, const dia::SourcePosition&);
		static Token makeSentinelEof(const dia::SourcePosition&);
		static Token makeSentinelBof(const dia::SourcePosition&);
		static Token makeKeyword(base::RawView keyword, const dia::SourcePosition&);
		static Token makeNumber(const base::RawView number, dia::SourcePosition);
		static Token makeString(base::RawView string, const dia::SourcePosition&);
		static Token makeChar(base::RawView string, const dia::SourcePosition&);
		static Token makeFormatString(
			Tokens&&                  tokens,
			Token&&                   sentinel_begin,
			Token&&                   sentinel_end,
			const dia::SourcePosition position
		);
		static Token makeFormatStringSubString(base::RawView string, const dia::SourcePosition);
		static Token
			makeBracketGroup(BracketType bracket_type, Tokens&& tokens, Token&& sentinel_begin, Token&& sentinel_end, const dia::SourcePosition&);
		static Token makeComment(base::RawView comment, const dia::SourcePosition&);
		static Token makeOperator(base::RawView oper, const dia::SourcePosition&);
		static Token makeIdentifier(base::RawView identifier, const dia::SourcePosition&);
		static Token makeSpecial(base::RawView identifier, const dia::SourcePosition&);
		static Token makeNumLiteral(base::RawView literal, const dia::SourcePosition&);
		static Token makeTypeSpecifier(base::RawView literal, const dia::SourcePosition&);
		static Token
			makeNumLiteralGroup(base::RawView full_view, Token&& value, Token&& specifier, const dia::SourcePosition&);
		static Token makeNumLiteralGroup(base::RawView full_view, Token&& value, const dia::SourcePosition&);
		/**@}*/

		/**
		 * @brief Makes a sentinel that copies the basic characteristics of the token. The copied
		 * characteristics are only the position.
		 */
		[[nodiscard]]
		Token asSentinel() const;

		Token() = delete;

		Token(const Token& other) = default;
		Token(Token&& other) noexcept;
		Token(Type type, base::RawView value, const dia::SourcePosition& position);
		Token(
			Type                       type,
			Tokens&&                   recursive,
			Token&&                    sentinel_begin,
			Token&&                    sentinel_end,
			const dia::SourcePosition& position,
			BracketType                bracket
		);
		Token(
			Type                       type,
			Tokens&&                   recursive,
			Token&&                    sentinel_begin,
			Token&&                    sentinel_end,
			const dia::SourcePosition& position
		);
		Token(
			Type type, base::RawView value, Tokens&& recursive, const dia::SourcePosition& position
		);

		friend void swap(Token& first, Token& second) noexcept;
		Token&      operator=(Token&& other) noexcept;


		[[nodiscard]]
		Type getType() const;
		[[nodiscard]]
		BracketType getBracketType() const;
		[[nodiscard]]
		base::StrID getValue() const;
		[[nodiscard]]
		std::string_view getStrValue() const;
		[[nodiscard]]
		const Tokens& getRecursive() const;
		[[nodiscard]]
		const Token& getSentinelBegin() const;
		[[nodiscard]]
		const Token& getSentinelEnd() const;

		[[nodiscard]]
		bool               isBracketGroup() const;
		[[nodiscard]] bool isBracketGroup(BracketType) const;

		[[nodiscard]]
		bool isRecursive() const;

		[[nodiscard]]
		bool isSpecial() const;
		[[nodiscard]]
		Special asSpecial() const;

		[[nodiscard]]
		bool isKeyword() const;
		[[nodiscard]]
		Keyword asKeyword() const;

		/**
		 * @brief Checks if the token is a proper operator (non-text)
		 */
		[[nodiscard]]
		bool isOperatorSymbol() const;
		/**
		 * @brief Checks if the token is a symbol or text operator
		 */
		[[nodiscard]]
		bool isOperatorSymbolOrText() const;
		/**
		 * @brief Checks if the token is a prefix operator(any symbol operator and a limited number
		 * of keyword text operators)
		 */
		[[nodiscard]]
		bool isPrefixOperator() const;
		/**
		 * @brief Converts the internal value to the named operator enum
		 */
		[[nodiscard]]
		NamedOperator asNamedOperator() const;
		/**
		 * @brief Converts the token to an operator if it's a binary operator
		 */
		[[nodiscard]]
		base::Optional<Operator> asBinaryOperator() const;
		/**
		 * @brief Converts the token to an operator if it's a prefix operator
		 */
		[[nodiscard]]
		base::Optional<Operator> asPrefixOperator() const;
		/**
		 * @brief Converts the token to an operator if it's a suffix operator
		 */
		[[nodiscard]]
		base::Optional<Operator> asSuffixOperator() const;

		[[nodiscard]]
		bool isIdentifier() const;
		[[nodiscard]]
		bool isNumLiteral() const;
		[[nodiscard]]
		bool isNumLiteralGroup() const;
		[[nodiscard]]
		bool isTypeSpecifier() const;
		[[nodiscard]]
		bool isComment() const;
		[[nodiscard]]
		bool isString() const;
		[[nodiscard]]
		bool isFormatString() const;
		[[nodiscard]]
		bool isChar() const;

		/**
		 * @brief Whether the token's value is literal text: a string, char, format-string part or
		 * comment. Such a token is never a keyword, special or operator, whatever its text is.
		 */
		[[nodiscard]]
		bool isLiteralText() const;

		[[nodiscard]] bool is(Type) const;
		/** @brief Whether the token is the special `spc`; never true for literal text. */
		[[nodiscard]] bool is(Special) const;
		/** @brief Whether the token is the operator `op`; never true for literal text. */
		[[nodiscard]] bool is(Operator) const;
		/** @brief Whether the token is the keyword `key`; never true for literal text. */
		[[nodiscard]] bool is(Keyword) const;

		[[nodiscard]]
		bool isStr(base::StrID str) const;

		[[nodiscard]]
		dia::SourcePosition getPosition() const;

		/**
		 * @brief Returns a human readable description of the token, used for error messages.
		 */
		[[nodiscard]]
		std::string describe() const;

	private:
		static Token makeError(const dia::SourcePosition&);

		Type                         type = Type::Empty;
		base::StrID                  str_id;
		Tokens                       recursive;
		std::shared_ptr<const Token> sentinel_begin;
		std::shared_ptr<const Token> sentinel_end;
		dia::SourcePosition          source_position;
		BracketType                  bracket_type{ BracketType::None };
	};

	/**
	 * @brief A basic wrapper for tokenization result
	 */
	struct TokenData final {
		Tokens tokens;
		Token  bof_sentinel;
		Token  eof_sentinel;

		TokenData() = delete;
		TokenData(TokenData&&) noexcept;
		TokenData(Tokens&& tokens, Token&& bof_sentinel, Token&& eof_sentinel);

		void       operator=(const TokenData&) = delete;
		TokenData& operator=(TokenData&&)      = default;

		~TokenData();
	};
}
