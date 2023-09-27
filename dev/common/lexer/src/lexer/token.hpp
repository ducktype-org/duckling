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
#include "source_position.hpp"
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

	struct TokenData {
		Tokens          tokens;
		fs::FileContent file_content;

		TokenData() = default;
		TokenData(TokenData &&) noexcept;
		TokenData(Tokens tokens, fs::FileContent file_content);

		void       operator=(const TokenData &) = delete;
		TokenData &operator=(TokenData &&)      = default;

		virtual ~TokenData();
	};

	class Token {
	public:
		enum class Type {
			Keyword,
			Identifier,
			NumLiteral,
			String,           // special group, changes lexing rules
			FormattedString,  // special group, changes lexing rules
			RoundGroup,       // (...)
			SquareGroup,      // [...]
			CurlyGroup,       // {...}
			AngleGroup,       // currently not used
			Operator,
			Comment,
			Special,
			Empty,
			Sentinel,
			Error
		};

		constexpr static std::array<Type, 6> non_terminal_tokens
			= { Type::String,      Type::FormattedString, Type::RoundGroup,
			    Type::SquareGroup, Type::CurlyGroup,      Type::AngleGroup };

		static Token makeSentinel();

		static Token makeKeyword(base::RawView keyword, const SourcePosition &);
		static Token makeNumber(const base::RawView number, SourcePosition);
		static Token makeString(base::RawView string, const SourcePosition &);
		static Token makeFormattedString(Tokens &&tokens, SourcePosition);
		static Token makeGroup(Char::ParType groupType, Tokens &&tokens, const SourcePosition &);
		static Token makeComment(base::RawView comment, const SourcePosition &);
		static Token makeOperator(base::RawView oper, const SourcePosition &);
		static Token makeIdentifier(base::RawView identifier, const SourcePosition &);
		static Token makeSpecial(base::RawView identifier, const SourcePosition &);
		static Token makeNumLiteral(base::RawView literal, const SourcePosition &);

		virtual ~Token() = default;

		Token() noexcept          = default;
		Token(const Token &other) = default;
		Token(Token &&other) noexcept;
		Token(Type type, base::RawView value, SourcePosition position);
		Token(Type type, Tokens &&recursive, SourcePosition position);
		friend void swap(Token &first, Token &second);
		Token      &operator=(Token other);

		[[nodiscard]]
		Type getType() const;
		[[nodiscard]]
		base::StrId getValue() const;
		[[nodiscard]]
		std::string_view getStrValue() const;
		[[nodiscard]]
		const Tokens &getRecursive() const;

		[[nodiscard]]
		bool isGroup() const;

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
		SourcePosition getPosition() const;

	private:
		static Token makeError(const SourcePosition &);

		Type           type = Type::Empty;
		base::StrId    str_id;
		Tokens         recursive;
		SourcePosition source_position;
	};
}
