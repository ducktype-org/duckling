/**
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "token.hpp"

#include <rift_definitions/key_spec_op.hpp>
#include <utility>
#include <algorithm>
#include <unicode/uchar.h>

namespace lexer {
	Token::Token(Token::Type type, const base::RawView value, SourcePosition position):
		  type(type),
		  str_id(value),
		  source_position(std::move(position)) {}

	Token::Token(Token::Type type, Tokens&& recursive, Token&& sentinel, SourcePosition position, UChar32 bracketType):
		  type(type),
		  recursive(std::move(recursive)),
		  sentinel(new Token(std::move(sentinel))),
		  source_position(std::move(position)),
		  bracket_type(bracketType) {
			if (type == Token::Type::BracketGroup) {
				std::string s;
				icu::UnicodeString(bracket_type).append(u_getBidiPairedBracket(bracket_type)).toUTF8String(s);
				str_id = base::StrId(base::RawView(s.data()));
			}
		  }


	Token Token::makeSentinelEnd(base::RawView view, const SourcePosition& pos) {
		return { Type::Sentinel, view, pos};
	}

	Token Token::makeSentinelEof(const SourcePosition& pos) {
		return { Type::Sentinel, base::RawView("EOF"), pos };
	}

	Token Token::makeComment(const base::RawView comment, const SourcePosition& position) {
		return { Type::Comment, comment, position };
	}

	Token Token::makeOperator(
		const base::RawView oper, const SourcePosition& position
	) {  // operator is keyword
		return { Type::Operator, oper, position };
	}

	Token Token::makeIdentifier(const base::RawView identifier, const SourcePosition& position) {
		if (rift_def::strAsKeyword(base::StrId(identifier)) != Keyword::NotAKeyword)
			return makeKeyword(identifier, position);
		return { Type::Identifier, identifier, position };
	}

	Token Token::makeKeyword(const base::RawView keyword, const SourcePosition& position) {
		return { Type::Keyword, keyword, position };
	}

	Token Token::makeSpecial(const base::RawView identifier, const SourcePosition& position) {
		return { Type::Special, identifier, position };
	}

	Token Token::makeNumLiteral(const base::RawView literal, const SourcePosition& position) {
		return { Type::NumLiteral, literal, position };
	}

	Token Token::makeString(const base::RawView string, const SourcePosition& position) {
		return { Type::String, string, position };
	}

	Token
		Token::makeGroup(UChar32 groupType, Tokens&& tokens, Token&& sentinel, const SourcePosition& position) {
			// for now doesn't fail on bad groupTypes
			return { Type::BracketGroup, std::move(tokens), std::move(sentinel), position, groupType };
	}

	Token Token::makeError(const SourcePosition& position) {
		return Token(Type::Error, base::RawView("<error>"), position);
	}

	void swap(Token& first, Token& second) {
		using std::swap;

		swap(first.recursive, second.recursive);
		swap(first.str_id, second.str_id);
		swap(first.type, second.type);
		swap(first.source_position, second.source_position);
		swap(first.sentinel, second.sentinel);
		swap(first.bracket_type, second.bracket_type);
	}

	Token& Token::operator=(Token other) {
		swap(*this, other);
		return *this;
	}

	Token::Token(Token&& other) noexcept: source_position(other.source_position) { swap(*this, other); }

	Token::Type Token::getType() const { return type; }

	UChar32 Token::getBracketType() const { return bracket_type; }

	base::StrId Token::getValue() const { return str_id; }

	std::string_view Token::getStrValue() const { return getValue().strView(); }

	const Tokens& Token::getRecursive() const { return recursive; }

	const Token& Token::getSentinel() const { 
		RIFT_ASSERT(isGroup(), "getSentinel called on non group token"); 
		return *sentinel; 
	}

	bool Token::isGroup() const {
		return type == Type::BracketGroup;
	}

	bool Token::isGroup(UChar32 group_type) const {
		return isGroup() && bracket_type == group_type;
	}

	bool Token::isTerminal() const {
		return std::find(non_terminal_tokens.begin(), non_terminal_tokens.end(), type)
		    == non_terminal_tokens.end();
	}

	bool Token::isNotTerminal() const { return !isTerminal(); }

	bool Token::isSpecial() const { return type == Type::Special; }

	Special Token::asSpecial() const { return rift_def::strAsSpecial(str_id); }

	bool Token::isKeyword() const { return type == Type::Keyword; }

	Keyword Token::asKeyword() const { return rift_def::strAsKeyword(str_id); }

	bool Token::isOperator() const { return type == Type::Operator; }

	bool Token::isIdentifier() const { return type == Type::Identifier; }

	bool Token::isNumLiteral() const { return type == Type::NumLiteral; }

	bool Token::isComment() const { return type == Type::Comment; }

	bool Token::isString() const { return type == Type::String; }

	bool Token::isStr(base::StrId str) const { return getValue() == str; }

	bool Token::is(Type type) const { return this->type == type; }

	bool Token::is(Operator op) const { return rift_def::strAsOperator(str_id) == op; }

	bool Token::is(Special spc) const { return rift_def::strAsSpecial(str_id) == spc; }

	bool Token::is(Keyword key) const { return rift_def::strAsKeyword(str_id) == key; }

	SourcePosition Token::getPosition() const { return source_position; }

	TokenData::TokenData(Tokens&& tokens, Token&& eof_sentinel, fs::FileContent file_content):
		  tokens(std::move(tokens)),
		  eof_sentinel(std::move(eof_sentinel)),
		  file_content(std::move(file_content)) {}

	TokenData::TokenData(TokenData&& oth) noexcept:
		  tokens(std::move(oth.tokens)),
		  eof_sentinel(std::move(oth.eof_sentinel)),
		  file_content(std::move(oth.file_content)){};

	TokenData::~TokenData() = default;
}
