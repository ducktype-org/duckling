/**
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <string>
#include <vector>

#include "char.hpp"
#include "diagnostic/source_position.hpp"
#include <base/smart_pointers.hpp>
#include <base/raw_view.hpp>
#include <base/string_id.hpp>
#include <filesystem/file.hpp>
#include <rift_definitions/key_spec_op.hpp>

namespace lexer {
	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	class Token;
	/**
	 * @brief Type representing a list of tokens
	 *
	 */
	using Tokens = std::vector<lexer::Token>;

	/**
	 * @brief Class representing a single token and providing methods of accessing information about
	 * it
	 *
	 * @todo Implement formatted string support
	 */
	class Token {
	public:
		enum class Type {
			Keyword,
			Identifier,
			NumLiteral,
			String,
			FormattedString,  ///< group
			BracketGroup,     ///< group storing opening bracket value in group_type
			Operator,
			Comment,
			Special,
			Empty,
			Sentinel,
			Error
		};

		/**
		 * @brief Non-exhaustive enum of bracket types
		 *
		 */
		enum BracketType : UChar32 {
			None   = 0,
			Round  = '(',
			Square = '[',
			Curly  = '{',
			Angle  = 0x30'08,
		};

		/**
		 * @name Functions that construct a Token
		 * @{
		 */
		static Token makeSentinelEnd(base::RawView view, const dia::SourcePosition&);
		static Token makeSentinelEof(const dia::SourcePosition&);
		static Token makeKeyword(base::RawView keyword, const dia::SourcePosition&);
		static Token makeNumber(const base::RawView number, dia::SourcePosition);
		static Token makeString(base::RawView string, const dia::SourcePosition&);
		static Token makeFormattedString(Tokens&& tokens, dia::SourcePosition);  ///< Unimplemented
		static Token
			makeBracketGroup(BracketType bracket_type, Tokens&& tokens, Token&& sentinel, const dia::SourcePosition&);
		static Token makeComment(base::RawView comment, const dia::SourcePosition&);
		static Token makeOperator(base::RawView oper, const dia::SourcePosition&);
		static Token makeIdentifier(base::RawView identifier, const dia::SourcePosition&);
		static Token makeSpecial(base::RawView identifier, const dia::SourcePosition&);
		static Token makeNumLiteral(base::RawView literal, const dia::SourcePosition&);
		/**@}*/

		virtual ~Token() = default;
		Token()          = delete;

		Token(const Token& other) = default;
		Token(Token&& other) noexcept;
		Token(Type type, base::RawView value, const dia::SourcePosition& position);
		Token(
			Type                       type,
			Tokens&&                   recursive,
			Token&&                    sentinel,
			const dia::SourcePosition& position,
			BracketType                bracket
		);
		friend void swap(Token& first, Token& second);
		Token&      operator=(Token&& other) noexcept;

		[[nodiscard]]
		Type getType() const;
		[[nodiscard]]
		BracketType getBracketType() const;
		[[nodiscard]]
		base::StrId getValue() const;
		[[nodiscard]]
		std::string_view getStrValue() const;
		[[nodiscard]]
		const Tokens& getRecursive() const;
		[[nodiscard]]
		const Token getSentinel() const;

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

		[[nodiscard]]
		bool isOperator() const;
		[[nodiscard]]
		bool isIdentifier() const;
		[[nodiscard]]
		bool isNumLiteral() const;
		[[nodiscard]]
		bool isComment() const;
		[[nodiscard]]
		bool isString() const;

		[[nodiscard]] bool is(Type) const;
		[[nodiscard]] bool is(Special) const;
		[[nodiscard]] bool is(Operator) const;
		[[nodiscard]] bool is(Keyword) const;

		[[nodiscard]]
		bool isStr(base::StrId str) const;

		[[nodiscard]]
		dia::SourcePosition getPosition() const;

	private:
		static Token makeError(const dia::SourcePosition&);

		Type                         type = Type::Empty;
		base::StrId                  str_id;
		Tokens                       recursive;
		std::shared_ptr<const Token> sentinel;
		dia::SourcePosition          source_position;
		BracketType                  bracket_type{ BracketType::None };
	};

	/**
	 * @brief A basic wrapper for tokenization result
	 */
	struct TokenData {
		Tokens tokens;
		Token  eof_sentinel;

		TokenData() = delete;
		TokenData(TokenData&&) noexcept;
		TokenData(Tokens&& tokens, Token&& eof_sentinel);

		void       operator=(const TokenData&) = delete;
		TokenData& operator=(TokenData&&)      = default;

		virtual ~TokenData();
	};
}
