/** 
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>

#include "char.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <base/string_id.hpp>
#include <base/raw_view.hpp>
#include <filesystem/file.hpp>

namespace lexer {
	class Token;
}
using Tokens = std::vector<lexer::Token>;

namespace lexer {
	using rift_def::Keyword;
	using rift_def::Special;
	using rift_def::Operator;

	struct TokenData {
		Tokens tokens;
		fs::FileContent file_content;
		
		TokenData() = default;
		TokenData(TokenData&&) noexcept;
		TokenData(Tokens tokens, fs::FileContent file_content);
		
		void operator=(const TokenData&) = delete;
		void operator=(TokenData&&) noexcept;
		
		virtual ~TokenData();
	};

	class Token {
		public:
			enum class Type {
				Keyword,
				Identifier,
				NumLiteral,
				String, // special group, changes lexing rules
				FormattedString, // special group, changes lexing rules
				RoundGroup, // (...)
				SquareGroup, // [...]
				CurlyGroup, // {...}
				AngleGroup, // currently not used
				Operator,
				Comment,
				Special,
				Empty,
				Sentinel,
				Error
			};

			struct Position {
				size_t line, column, raw;
				std::string str() const {
					return std::to_string(line) + ":" + std::to_string(column);
				}
			};

			constexpr static std::array<Type, 6> non_terminal_tokens = 
				{Type::String, Type::FormattedString, Type::RoundGroup,
				 Type::SquareGroup, Type::CurlyGroup, Type::AngleGroup};

			static Token makeSentinel();
			
			static Token makeKeyword(const base::RawView keyword, Position);
			static Token makeNumber(const base::RawView number, Position);
			static Token makeString(const base::RawView string, Position);
			static Token makeFormattedString(Tokens&& tokens, Position);
			static Token makeGroup(Char::ParType groupType, Tokens&& tokens, Position);
			static Token makeComment(const base::RawView comment, Position);
			static Token makeOperator(const base::RawView oper, Position);
			static Token makeIdentifier(const base::RawView identifier, Position);
			static Token makeSpecial(const base::RawView identifier, Position);
			static Token makeNumLiteral(const base::RawView literal, Position);
			
			virtual ~Token() = default;
			
			Token() noexcept = default;
			Token(const Token& other) = default;
			Token(Type type, const base::RawView value, Position);
			Token(Type type, Tokens&& recursive, Position);
			friend void swap(Token& first, Token& second);
			Token& operator = (Token other);
			Token(Token&& other) noexcept;

			Type getType() const;
			base::StrId getValue() const;
			std::string_view getStrValue() const;
			const Tokens& getRecursive() const;

			bool isGroup() const;

			bool isTerminal() const;
			bool isNotTerminal() const;

			bool isSpecial() const;
			Special asSpecial() const;

			bool isKeyword() const;
			Keyword asKeyword() const;

			bool isOperator() const;
			bool isIdentifier() const;
			bool isNumLiteral() const;
			bool isComment() const;
			bool isString() const;

			bool is(Type) const;
			bool is(Special) const;
			bool is(Operator) const;
			bool is(Keyword) const;

			bool isStr(base::StrId str) const;

			Position getPosition() const;

		private:
			static Token makeError(Position);

			Type type = Type::Empty;
			base::StrId str_id;
			Tokens recursive;
			Position position;
	};
}
