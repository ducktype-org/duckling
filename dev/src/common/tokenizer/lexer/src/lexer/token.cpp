/**
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "token.hpp"

#include <lang_definitions/key_spec_op.hpp>

#include <unicode/uchar.h>

#include <utility>

namespace lexer {
	std::string Token::typeToStr(Type type) {
		switch (type) {
		case Type::Keyword:
			return "Keyword";
		case Type::Identifier:
			return "Identifier";
		case Type::NumLiteral:
			return "NumLiteral";
		case Type::NumLiteralGroup:
			return "NumLiteralGroup";
		case Type::TypeSpecifier:
			return "TypeSpecifier";
		case Type::String:
			return "String";
		case Type::Char:
			return "Char";
		case Type::FormatString:
			return "FormatString";
		case Type::FormatStringSubString:
			return "FormatStringSubString";
		case Type::BracketGroup:
			return "BracketGroup";
		case Type::Operator:
			return "Operator";
		case Type::Comment:
			return "Comment";
		case Type::Special:
			return "Special";
		case Type::Empty:
			return "Empty";
		case Type::Sentinel:
			return "Sentinel";
		case Type::Error:
			return "Error";
		}
		CORE_UNREACHABLE();
	}

	Token::Token(Token::Type type, const base::RawView value, const dia::SourcePosition& position):
		  type(type),
		  str_id(value),
		  source_position(position) {}

	Token::Token(
		Type type, base::RawView value, Tokens&& recursive, const dia::SourcePosition& position
	):
		  type(type),
		  str_id(value),
		  recursive(std::move(recursive)),
		  source_position(position) {
		CORE_ASSERT(
			type == Type::NumLiteralGroup || type == Type::FormatString,
			"Recursive token constructor called on non recursive token type"
		);
	}

	Token::Token(
		Token::Type                type,
		Tokens&&                   recursive,
		Token&&                    sentinel_begin,
		Token&&                    sentinel_end,
		const dia::SourcePosition& position,
		BracketType                bracket_type
	):
		  type(type),
		  recursive(std::move(recursive)),
		  sentinel_begin(new Token(std::move(sentinel_begin))),
		  sentinel_end(new Token(std::move(sentinel_end))),
		  source_position(position),
		  bracket_type(bracket_type) {
		CORE_ASSERT(
			this->sentinel_begin->getType() == Type::Sentinel,
			"non-sentinel token passed as sentinel"
		);
		CORE_ASSERT(
			this->sentinel_end->getType() == Type::Sentinel, "non-sentinel token passed as sentinel"
		);
		CORE_ASSERT(
			type == Type::BracketGroup, "non-bracket token created with bracket constructor"
		);

		// sets str_id of brackets to the pair of brackets for example "()"
		std::string s;
		icu::UnicodeString(bracket_type).append(u_getBidiPairedBracket(bracket_type)).toUTF8String(s);
		str_id = base::StrID(base::RawView(s.data()));
	}

	Token::Token(
		Token::Type                type,
		Tokens&&                   recursive,
		Token&&                    sentinel_begin,
		Token&&                    sentinel_end,
		const dia::SourcePosition& position
	):
		  type(type),
		  recursive(std::move(recursive)),
		  sentinel_begin(new Token(std::move(sentinel_begin))),
		  sentinel_end(new Token(std::move(sentinel_end))),
		  source_position(position) {
		CORE_ASSERT(
			this->sentinel_begin->getType() == Type::Sentinel,
			"non-sentinel token passed as sentinel"
		);
		CORE_ASSERT(
			this->sentinel_end->getType() == Type::Sentinel, "non-sentinel token passed as sentinel"
		);
		CORE_ASSERT(type == Type::FormatString, "This constructor is only used with format strings");
		str_id = base::StrID(base::RawView("f\"\""));
	}

	Token Token::makeSentinel(base::RawView view, const dia::SourcePosition& pos) {
		return { Type::Sentinel, view, pos };
	}

	Token Token::makeSentinelEof(const dia::SourcePosition& pos) {
		return { Type::Sentinel, base::RawView("EOF"), pos };
	}

	Token Token::makeSentinelBof(const dia::SourcePosition& pos) {
		return { Type::Sentinel, base::RawView("BOF"), pos };
	}

	Token Token::makeComment(const base::RawView comment, const dia::SourcePosition& position) {
		return { Type::Comment, comment, position };
	}

	Token Token::makeOperator(
		const base::RawView oper, const dia::SourcePosition& position
	) {  // operator is keyword
		return { Type::Operator, oper, position };
	}

	Token Token::makeIdentifier(const base::RawView identifier, const dia::SourcePosition& position) {
		if (lang_def::strAsKeyword(base::StrID(identifier)) != Keyword::NotAKeyword)
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

	Token Token::makeTypeSpecifier(const base::RawView literal, const dia::SourcePosition& position) {
		return { Type::TypeSpecifier, literal, position };
	}

	Token Token::makeFormatString(
		Tokens&&                  tokens,
		Token&&                   sentinel_begin,
		Token&&                   sentinel_end,
		const dia::SourcePosition position
	) {
		return {
			Type::FormatString,      std::move(tokens), std::move(sentinel_begin),
			std::move(sentinel_end), position,
		};
	}

	Token Token::makeFormatStringSubString(base::RawView string, const dia::SourcePosition position) {
		return { Type::FormatStringSubString, string, position };
	}

	Token Token::makeNumLiteralGroup(
		base::RawView full_view, Token&& value, Token&& specifier, const dia::SourcePosition& position
	) {
		CORE_ASSERT(value.isNumLiteral(), "Value in literal group has to be a NumLiteral");
		CORE_ASSERT(
			specifier.isTypeSpecifier(), "Specifier in literal group has to be a TypeSpecifier"
		);
		Tokens sub_tokens{ std::move(value), std::move(specifier) };
		return { Type::NumLiteralGroup, full_view, std::move(sub_tokens), position };
	}

	Token Token::makeNumLiteralGroup(
		base::RawView full_view, Token&& value, const dia::SourcePosition& position
	) {
		CORE_ASSERT(value.isNumLiteral(), "Value in literal group has to be a NumLiteral");
		return { Type::NumLiteralGroup, full_view, { std::move(value) }, position };
	}

	Token Token::makeString(const base::RawView string, const dia::SourcePosition& position) {
		return { Type::String, string, position };
	}

	Token Token::makeChar(const base::RawView string, const dia::SourcePosition& position) {
		return { Type::Char, string, position };
	}

	Token Token::makeBracketGroup(
		BracketType                group_type,
		Tokens&&                   tokens,
		Token&&                    sentinel_begin,
		Token&&                    sentinel_end,
		const dia::SourcePosition& position
	) {
		return { Type::BracketGroup,      std::move(tokens), std::move(sentinel_begin),
			     std::move(sentinel_end), position,          group_type };
	}

	Token Token::asSentinel() const { return { Type::Sentinel, "", source_position }; }

	Token Token::makeError(const dia::SourcePosition& position) {
		return { Type::Error, base::RawView("<error>"), position };
	}

	void swap(Token& first, Token& second) noexcept {
		using std::swap;

		swap(first.recursive, second.recursive);
		swap(first.str_id, second.str_id);
		swap(first.type, second.type);
		swap(first.source_position, second.source_position);
		swap(first.sentinel_begin, second.sentinel_begin);
		swap(first.sentinel_end, second.sentinel_end);
		swap(first.bracket_type, second.bracket_type);
	}

	Token& Token::operator=(Token&& other) noexcept {
		swap(*this, other);
		return *this;
	}

	Token::Token(Token&& other) noexcept: source_position(other.source_position) {
		swap(*this, other);
	}

	Token::Type Token::getType() const { return type; }

	Token::BracketType Token::getBracketType() const { return bracket_type; }

	base::StrID Token::getValue() const { return str_id; }

	std::string_view Token::getStrValue() const { return getValue().strView(); }

	const Tokens& Token::getRecursive() const { return recursive; }

	const Token& Token::getSentinelBegin() const {
		CORE_ASSERT(isRecursive(), "getSentinel called on non-recursive token");
		CORE_ASSERT(sentinel_begin, "unassigned sentinel in recursive token");
		return *sentinel_begin;
	}

	const Token& Token::getSentinelEnd() const {
		CORE_ASSERT(isRecursive(), "getSentinel called on non-recursive token");
		CORE_ASSERT(sentinel_end, "unassigned sentinel in recursive token");
		return *sentinel_end;
	}

	bool Token::isBracketGroup() const { return type == Type::BracketGroup; }

	bool Token::isBracketGroup(BracketType btype) const {
		return isBracketGroup() && bracket_type == btype;
	}

	bool Token::isRecursive() const {
		return type == Type::BracketGroup || type == Type::FormatString
		    || type == Type::NumLiteralGroup;
	}

	bool Token::isSpecial() const { return type == Type::Special; }

	Special Token::asSpecial() const { return lang_def::strAsSpecial(str_id); }

	bool Token::isKeyword() const { return type == Type::Keyword; }

	Keyword Token::asKeyword() const { return lang_def::strAsKeyword(str_id); }

	bool Token::isOperatorSymbol() const { return type == Type::Operator; }

	bool Token::isOperatorSymbolOrText() const {
		return isOperatorSymbol() || isKeyword() || isIdentifier();
	}

	bool Token::isPrefixOperator() const {
		return isOperatorSymbol()
		    || (isKeyword()
		        && lang_def::keywordFlags(asKeyword())
		               .contains(lang_def::KeywordFlagsOptions::IsGenPrefixOp));
	}

	NamedOperator Token::asNamedOperator() const {
		if (isOperatorSymbol()) return Operator(getValue()).asNamed();
		return lang_def::NamedOperator::NotAnOperator;
	}

	base::Optional<Operator> Token::asBinaryOperator() const {
		if (isOperatorSymbolOrText()) return { { getValue() } };
		return {};
	}

	base::Optional<Operator> Token::asPrefixOperator() const {
		if (isPrefixOperator()) return { { getValue() } };
		return {};
	}

	base::Optional<Operator> Token::asSuffixOperator() const { return asBinaryOperator(); }

	bool Token::isIdentifier() const { return type == Type::Identifier; }

	bool Token::isNumLiteral() const { return type == Type::NumLiteral; }

	bool Token::isNumLiteralGroup() const { return type == Type::NumLiteralGroup; }

	bool Token::isTypeSpecifier() const { return type == Type::TypeSpecifier; }

	bool Token::isComment() const { return type == Type::Comment; }

	bool Token::isString() const { return type == Type::String; }

	bool Token::isFormatString() const { return type == Type::FormatString; }

	bool Token::isChar() const { return type == Type::Char; }

	bool Token::isStr(base::StrID str) const { return getValue() == str; }

	bool Token::is(Type qtype) const { return type == qtype; }

	bool Token::is(Operator op) const { return op == getValue(); }

	bool Token::is(Special spc) const { return lang_def::strAsSpecial(str_id) == spc; }

	bool Token::is(Keyword key) const { return lang_def::strAsKeyword(str_id) == key; }

	dia::SourcePosition Token::getPosition() const { return source_position; }

	std::string Token::describe() const {
		return base::strConcat(typeToStr(type), " '", getStrValue(), "'");
	}

	TokenData::TokenData(Tokens&& tokens, Token&& bof_sentinel, Token&& eof_sentinel):
		  tokens(std::move(tokens)),
		  bof_sentinel(std::move(bof_sentinel)),
		  eof_sentinel(std::move(eof_sentinel)) {}

	TokenData::TokenData(TokenData&& oth) noexcept:
		  tokens(std::move(oth.tokens)),
		  bof_sentinel(std::move(oth.bof_sentinel)),
		  eof_sentinel(std::move(oth.eof_sentinel)) {}

	TokenData::~TokenData() = default;
}
