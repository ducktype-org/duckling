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
	Token::Token(Token::Type type, const base::RawView value, const dia::SourcePosition& position):
		  type(type),
		  str_id(value),
		  source_position(position) {}

	Token::Token(
		Token::Type         type,
		Tokens&&            recursive,
		Token&&             sentinel,
		const dia::SourcePosition& position,
		BracketType         bracket_type
	):
		  type(type),
		  recursive(std::move(recursive)),
		  sentinel(new Token(std::move(sentinel))),
		  source_position(position),
		  bracket_type(bracket_type) {
		RIFT_ASSERT(
			this->sentinel->getType() == Type::Sentinel, "non-sentinel token passed as sentinel"
		);
		RIFT_ASSERT(
			type == Type::BracketGroup, "non-bracket token created with bracket constructor"
		);

		// sets str_id of brackets to the pair of brackets for example "()"
		std::string s;
		icu::UnicodeString(bracket_type)
			.append(u_getBidiPairedBracket(bracket_type))
			.toUTF8String(s);
		str_id = base::StrId(base::RawView(s.data()));
	}

	Token Token::makeSentinelEnd(base::RawView view, const dia::SourcePosition& pos) {
		return { Type::Sentinel, view, pos };
	}

	Token Token::makeSentinelEof(const dia::SourcePosition& pos) {
		return { Type::Sentinel, base::RawView("EOF"), pos };
	}

	Token Token::makeComment(const base::RawView comment, const dia::SourcePosition& position) {
		return { Type::Comment, comment, position };
	}

	Token Token::makeOperator(
		const base::RawView oper, const dia::SourcePosition& position
	) {  // operator is keyword
		return { Type::Operator, oper, position };
	}

	Token
		Token::makeIdentifier(const base::RawView identifier, const dia::SourcePosition& position) {
		if (rift_def::strAsKeyword(base::StrId(identifier)) != Keyword::NotAKeyword)
			return makeKeyword(identifier, position);
		return { Type::Identifier, identifier, position };
	}

	Token Token::makeKeyword(const base::RawView keyword, const dia::SourcePosition& position) {
		return { Type::Keyword, keyword, position };
	}

	Token Token::makeSpecial(const base::RawView identifier, const dia::SourcePosition& position) {
		return { Type::Special, identifier, position };
	}

	Token Token::makeNumLiteral(const base::RawView literal, const dia::SourcePosition& position) {
		return { Type::NumLiteral, literal, position };
	}

	Token Token::makeString(const base::RawView string, const dia::SourcePosition& position) {
		return { Type::String, string, position };
	}

	Token Token::makeBracketGroup(
		BracketType                groupType,
		Tokens&&                   tokens,
		Token&&                    sentinel,
		const dia::SourcePosition& position
	) {
		return { Type::BracketGroup, std::move(tokens), std::move(sentinel), position, groupType };
	}

	Token Token::makeError(const dia::SourcePosition& position) {
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

	Token& Token::operator=(Token&& other)  noexcept {
		swap(*this, other);
		return *this;
	}

	Token::Token(Token&& other) noexcept: source_position(other.source_position) {
		swap(*this, other);
	}

	Token::Type Token::getType() const { return type; }

	Token::BracketType Token::getBracketType() const { return bracket_type; }

	base::StrId Token::getValue() const { return str_id; }

	std::string_view Token::getStrValue() const { return getValue().strView(); }

	const Tokens& Token::getRecursive() const { return recursive; }

	const Token Token::getSentinel() const {
		RIFT_ASSERT(isRecursive(), "getSentinel called on non-recursive token");
		RIFT_ASSERT(sentinel, "un assigned sentinel in recursive token");
		return Token(*sentinel);
	}

	bool Token::isBracketGroup() const { return type == Type::BracketGroup; }

	bool Token::isBracketGroup(BracketType type) const {
		return isBracketGroup() && bracket_type == type;
	}

	bool Token::isRecursive() const {
		return type == Type::BracketGroup || type == Type::FormattedString;
	}

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

	dia::SourcePosition Token::getPosition() const { return source_position; }

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
