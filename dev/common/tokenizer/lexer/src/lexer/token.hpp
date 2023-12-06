/**
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "char.hpp"
#include "source_position/source_position.hpp"
#include <base/raw_view.hpp>
#include <base/string_id.hpp>
#include <filesystem/file.hpp>
#include <rift_definitions/key_spec_op.hpp>

namespace lexer {
	class Token;
}

using Tokens = std::vector<lexer::Token>;

namespace lexer {
	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	/**
	 * @brief Class used to store token information
	 * 
	 * @todo Implement formatted string
	 */
	class Token {
	public:
		enum class Type {
			Keyword,
			Identifier,
			NumLiteral,
			String,           ///< special group, changes lexing rules
			FormattedString,  ///< special group, changes lexing rules
			BracketGroup,     ///< stores opening bracket value in bracket_type
			Operator,
			Comment,
			Special,
			Empty,
			Sentinel,
			Error
		};

		constexpr static std::array<Type, 3> non_terminal_tokens
			= { Type::String,      Type::FormattedString, Type::BracketGroup};


		/**
		 * @name Functions that construct Tokens
		 * @{
		 */
		static Token makeSentinelEnd(base::RawView view, const dia::SourcePosition&);
		static Token makeSentinelEof(const dia::SourcePosition&);
		static Token makeKeyword(base::RawView keyword, const dia::SourcePosition&);
		static Token makeNumber(const base::RawView number, dia::SourcePosition);
		static Token makeString(base::RawView string, const dia::SourcePosition&);
		static Token makeFormattedString(Tokens&& tokens, dia::SourcePosition);
		static Token makeGroup(UChar32 groupType, Tokens&& tokens, Token&& sentinel, const dia::SourcePosition&);
		static Token makeComment(base::RawView comment, const dia::SourcePosition&);
		static Token makeOperator(base::RawView oper, const dia::SourcePosition&);
		static Token makeIdentifier(base::RawView identifier, const dia::SourcePosition&);
		static Token makeSpecial(base::RawView identifier, const dia::SourcePosition&);
		static Token makeNumLiteral(base::RawView literal, const dia::SourcePosition&);
		/**@}*/

		virtual ~Token() = default;

		Token(const Token& other) = default;
		Token(Token&& other) noexcept;
		Token(Type type, base::RawView value, dia::SourcePosition position);
		Token(Type type, Tokens&& recursive, Token&& sentinel, dia::SourcePosition position, UChar32 bracket);
		friend void swap(Token& first, Token& second);
		Token&      operator=(Token other);

		[[nodiscard]]
		Type getType() const;
		[[nodiscard]]
		UChar32 getBracketType() const;
		[[nodiscard]]
		base::StrId getValue() const;
		[[nodiscard]]
		std::string_view getStrValue() const;
		[[nodiscard]]
		const Tokens& getRecursive() const;
		[[nodiscard]]
		const Token getSentinel() const;

		[[nodiscard]]
		bool isGroup() const;
		[[nodiscard]]
		bool isGroup(UChar32 group_type) const;

		[[nodiscard]]
		bool isTerminal() const;
		[[nodiscard]]
		bool isNotTerminal() const;

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
		Token() noexcept = default;

		static Token makeError(const dia::SourcePosition&);

		Type           type = Type::Empty;
		base::StrId    str_id;
		Tokens         recursive;
		std::shared_ptr<Token> sentinel;
		dia::SourcePosition source_position;
		UChar32 bracket_type = 0;
	};

	/**
	 * @brief Class used to store a tokenized file
	 */
	struct TokenData {
		Tokens          tokens;
		Token           eof_sentinel;
		fs::FileContent file_content;

		TokenData() = default;
		TokenData(TokenData&&) noexcept;
		TokenData(Tokens&& tokens, Token&& eof_sentinel, fs::FileContent file_content);

		void       operator=(const TokenData&) = delete;
		TokenData& operator=(TokenData&&)      = default;

		virtual ~TokenData();
	};
}
