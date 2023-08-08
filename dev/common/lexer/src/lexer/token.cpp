/** 
 * @file token.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "token.hpp"

#include <rift_definitions/key_spec_op.hpp>
#include <utility>
#include <algorithm>

namespace lexer{
	Token Token::makeSentinel() {
		Position dummy_position = {static_cast<usize>(-1), -static_cast<usize>(1), static_cast<usize>(-1)};
		return Token(Type::Sentinel, base::RawView(""), dummy_position);
	}

	Token::Token(Token::Type type, const base::RawView value, Position position) : type(type), str_id(value), position(position) {}
	
	Token::Token(Token::Type type, Tokens&& recursive, Position position):
		type(type), recursive(recursive), position(position) {}
	
	Token Token::makeComment(const base::RawView comment, Position position){
		return Token(Type::Comment, comment, position);
	}

	Token Token::makeOperator(const base::RawView oper, Position position) { //operator is keyword
		return Token(Type::Operator, oper, position);
	}

	Token Token::makeIdentifier(const base::RawView identifier, Position position) {
		if (rift_def::strAsKeyword(base::StrId(identifier)) != Keyword::NotAKeyword) {
			return makeKeyword(identifier, position);
		}
		return Token(Type::Identifier, identifier, position);
	}

	Token Token::makeKeyword(const base::RawView keyword, Position position) {
		return Token(Type::Keyword, keyword, position);
	}

	Token Token::makeSpecial(const base::RawView identifier, Position position) {
		return Token(Type::Special, identifier, position);
	}
	
	Token Token::makeNumLiteral(const base::RawView literal, Position position) {
		return Token(Type::NumLiteral, literal, position);
	}

	Token Token::makeString(const base::RawView string, Position position) {
		return Token(Type::String, string, position);
	}

	Token Token::makeGroup(Char::ParType groupType, Tokens&& tokens, Position position) {
		switch (groupType){
			case Char::Round:
				return Token(Type::RoundGroup, std::move(tokens), position);
			case Char::Square:
				return Token(Type::SquareGroup, std::move(tokens), position);
			case Char::Curly:
				return Token(Type::CurlyGroup, std::move(tokens), position);
		
			case Char::Angle:
			case Char::NotAPar:
			default:
				return makeError(position);
		}
	}

	Token Token::makeError(Position position) {
		Token out;
		out.type = Type::Error;
		out.position = position;
		return out;
	}
			
	
	void swap(Token& first, Token& second){
		using std::swap;
		
		swap(first.recursive, second.recursive);
		swap(first.str_id, second.str_id);
		swap(first.type, second.type);
		swap(first.position, second.position);
	}
	
	Token& Token::operator = (Token other){
		swap(*this, other);
		return *this;
	}
	
	Token::Token(Token&& other) noexcept: Token() {
		swap(*this, other);
	}

	Token::Type Token::getType() const {
		return type;
	}

	base::StrId Token::getValue() const {
		return str_id;
	}

	std::string_view Token::getStrValue() const {
		return getValue().strView();
	}

	const Tokens& Token::getRecursive() const {
		return recursive;
	}

	bool Token::isGroup() const {
		return type == Type::AngleGroup
			|| type == Type::CurlyGroup
			|| type == Type::SquareGroup
			|| type == Type::RoundGroup;
	}

	bool Token::isTerminal() const {
		return std::find(non_terminal_tokens.begin(), non_terminal_tokens.end(), type) == non_terminal_tokens.end();
	}

	bool Token::isNotTerminal() const {
		return !isTerminal();
	}

	bool Token::isSpecial() const {
		return type == Type::Special;
	};

	Special Token::asSpecial() const {
		return rift_def::strAsSpecial(str_id);
	};

	bool Token::isKeyword() const {
		return type == Type::Keyword;
	};

	Keyword Token::asKeyword() const {
		return rift_def::strAsKeyword(str_id);
	};

	bool Token::isOperator() const {
		return type == Type::Operator;
	}

	bool Token::isIdentifier() const {
		return type == Type::Identifier;
	}

	bool Token::isNumLiteral() const {
		return type == Type::NumLiteral;
	}
	
	bool Token::isComment() const {
		return type == Type::Comment;
	}

	bool Token::isString() const {
		return type == Type::String;
	}

	bool Token::isStr(base::StrId str) const {
		return getValue() == str;
	}

	bool Token::is(Type type) const {
		return this->type == type;
	}

	bool Token::is(Operator op) const {
		return rift_def::strAsOperator(str_id) == op;
	}

	bool Token::is(Special spc) const {
		return rift_def::strAsSpecial(str_id) == spc;
	};

	bool Token::is(Keyword key) const {
		return rift_def::strAsKeyword(str_id) == key;
	};

	Token::Position Token::getPosition() const{
		return position;
	}

	TokenData::TokenData(Tokens tokens, fs::FileContent file_content): 
		tokens(std::move(tokens)), file_content(file_content) {}
	
	TokenData::TokenData(TokenData&& oth) noexcept:
		tokens(std::move(oth.tokens)),
		file_content(std::move(oth.file_content)) {};
	
	void TokenData::operator=(TokenData&& oth) noexcept {
		file_content = std::move(oth.file_content);
		tokens = std::move(oth.tokens);
	}

	TokenData::~TokenData() {}
}
