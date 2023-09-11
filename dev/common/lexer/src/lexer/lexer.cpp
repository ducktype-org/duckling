/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "lexer.hpp"
#include "lexer_class.hpp"
#include <iostream>
#include <rift_definitions/key_spec_op.hpp>

// @TODO: change name_ names to sth meaningfull

namespace lexer {

	// @TODO: Move it to a separate file...
	CharArray decode(base::RawView file_content) {
		// @TODO: deduce encoding
		CharArray out = lexer::decode<fs::UTF8>(file_content);
		//...
		return out;
	}

	Lexer::Lexer(const fs::FilePath& file)
		: file_(std::make_shared<fs::FilePath>(file)), fileContent_(file_->getContent()),
		  charArray_(decode(fileContent_.view())) {}

	Tokens Lexer::tokenize(bool dprint) {
		tokens_.clear();
		token_messages = dprint;
		codeblock();
		if (dprint) {
			// @TODO: better customization of this dprint
			console.print(std::cerr);
		}
		return std::move(tokens_);
	}

	void Lexer::next() {
		if (!isEOF()) {
			if (isEOL()) {
				++lineNumber_;
				columnNumber_ = 1;
			} else {
				columnNumber_++;
			}
		}
		++where_;
	}

	void Lexer::skip(usize n) {
		for (usize i = 0; i < n; i++) next();
	}

	bool Lexer::tryRawValue(char rawValue, usize fwd) const {
		return charArray_.getArray().size() > where_ + fwd && peek(fwd).isAsciiValue(rawValue);
	}

	const Char& Lexer::peek(usize fwd) const {
		return charArray_.get(where_ + fwd);
	}

	std::string Lexer::generateLineColumnInfo() const {
		return "(" + std::to_string(lineNumber_) + ":" + std::to_string(columnNumber_) + ")";
	}

	void Lexer::addTokenMsg(usize begin, usize end, std::string_view token_type,
	                        printer::MessageType message_type) {

		if (token_messages) {
			console.add(
				printer::Message({{"Add token: "},
			                      {std::string(token_type)},
			                      {"("},
			                      {std::string(charArray_.composeRaw(begin, end).stringView())},
			                      {")"}},
			                     message_type));
		}
	}

	void Lexer::codeblock() {
		parseCodeblockInto(tokens_);
	}

	void Lexer::parseCodeblockInto(Tokens& output) {
		while (!isEOF()) parseSingleInto(output);
	}

	void Lexer::parseSingleInto(Tokens& output) {
		usize begin = where_;
		SourcePosition sourcePosition(file_, lineNumber_, columnNumber_, where_);
		if (isEOF()) {
			// @TODO: error
			RIFT_PANIC("EOF encountered inside parseSingleInto");
		}
		// @TODO: for now comments aren't saved because it's way to hard to parse with the current
		// parser
		else if (isCommentBegin()) {
			usize end = comment(/*output*/);
			sourcePosition.setEnd(end);

			addTokenMsg(begin + 2, end, "line comment", printer::MessageType::DEBUG);
			// output.push_back(Token::makeComment(charArray_.composeRaw(begin + 2, end),
			// sourcePosition));
		} else if (isBlockCommentBegin()) {
			usize end = blockComment();
			sourcePosition.setEnd(end);

			addTokenMsg(begin + 2, end, "block comment", printer::MessageType::DEBUG);
			// output.push_back(Token::makeComment(charArray_.composeRaw(begin + 2, end),
			// sourcePosition));
		} else if (peek().isOperator()) {
			usize end = oper();
			sourcePosition.setEnd(end);
			addTokenMsg(begin, end, "operator", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeOperator(charArray_.composeRaw(begin, end), sourcePosition));
		} else if (peek().isCharacter()) {
			usize end = identifier();
			sourcePosition.setEnd(end);
			std::string message;
			output.push_back(
				Token::makeIdentifier(charArray_.composeRaw(begin, end), sourcePosition));
			if (output.back().getType() == Token::Type::Identifier) {
				addTokenMsg(begin, end, "identifier", printer::MessageType::DEBUG);
			} else if (output.back().getType() == Token::Type::Keyword) {
				addTokenMsg(begin, end, "keyword", printer::MessageType::DEBUG);
			}
		} else if (isStringBegin()) {
			usize end = string();
			sourcePosition.setEnd(end + 1);

			addTokenMsg(begin + 1, end, "string", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeString(charArray_.composeRaw(begin + 1, end), sourcePosition));
		} else if (peek().isSpecial()) {

			// the groups are constructed here
			if (peek().isParOpen()) {
				auto group_type = peek().getParType();
				console.add({{{"group begin"}}, printer::MessageType::DEBUG}); // @TODO: better
				Tokens inner_tokens = parGroup(group_type);

				if(inner_tokens.empty()) {
					sourcePosition.setEnd(where_ + 1);
				} else {
					auto lastEnd = inner_tokens.back().getPosition().getEnd();
					sourcePosition.setEnd(lastEnd + 1);
				}

				output.push_back(
					Token::makeGroup(group_type, std::move(inner_tokens), sourcePosition));
				console.add({{{"group end"}}, printer::MessageType::DEBUG});
			} else {
				usize end = special();
				sourcePosition.setEnd(end);
				addTokenMsg(begin, end, "special", printer::MessageType::DEBUG);
				output.push_back(
					Token::makeSpecial(charArray_.composeRaw(begin, end), sourcePosition));
			}
		} else if (peek().isDigit()) {
			usize end = numLiteral();
			sourcePosition.setEnd(end);

			addTokenMsg(begin, end, "numLiteral", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeNumLiteral(charArray_.composeRaw(begin, end), sourcePosition));
		} else {
			if (not peek().isWhitespace()) {
				console.add(printer::Message(
					{{"Skipped"},
				     {generateLineColumnInfo()},
				     {"("},
				     {std::string(charArray_.composeRaw(begin, begin).stringView())},
				     {")"}},
					printer::MessageType::DEBUG));
			}
			next(); // in else??
		}
	}

	// @TODO: think if we want to allow some kind of nested single line comments, the version
	// commented out below doesn't work
	usize Lexer::comment(/*Tokens& output*/) {
		skip(2); // "//"
		// usize begin = where_;
		// SourcePosition sourcePosition(file_, where_);
		// Token::Position position = {lineNumber_, columnNumber_, where_};
		while (true) {
			if (isEOF()) {
				return where_ - 1;
			} else if (isEOL()) {
				skip(1);
				return where_ - 2;
			}
			// else if(isCommentBegin()) {comment(output);}
			// else if(isBlockCommentBegin()) {
			// addTokenMsg(begin, where_ - 1, "line comment", printer::MessageType::DEBUG);
			// output.push_back(Token::makeComment(charArray_.composeRaw(begin, where_ - 1),
			// position)); return blockComment();
			// }
			else {
				next();
			}
		}
	}

	usize Lexer::blockComment() {
		skip(2); // "/*"
		while (true) {
			if (isEOF()) {
				console.add(printer::Message(
					{
						{"Missing end of block comment at "},
						{generateLineColumnInfo()},
					},
					printer::MessageType::WARNING));
				return where_ - 1;
			}
			// @FIXME: with the current way of adding tokens this doesn't add them as separate
			// comments just pairs starts and ends
			else if (isBlockCommentBegin()) {
				blockComment();
			} else if (isBlockCommentEnd()) {
				skip(2);
				return where_ - 3;
			} else {
				next();
			}
		}
	}

	usize Lexer::oper() {
		while (!isEOF() and peek().isOperator()) {
			next();
		}
		return where_ - 1;
	}

	usize Lexer::identifier() {
		next(); // first char - character
		while (!isEOF() and (peek().isCharacter() || peek().isDigit())) {
			next();
		}
		return where_ - 1;
	}

	usize Lexer::special() {
		next();
		return where_ - 1;
	}

	usize Lexer::numBinaryLiteral() {
		skip(2); // 0b
		while (!peek().isEOF()) {
			if (!peek().isAsciiValue('0') && !peek().isAsciiValue('1'))
				break;
			next();
		}

		return where_ - 1;
	}


	usize Lexer::numHexLiteral() {
		skip(2); // 0x

		while (!peek().isEOF()) {

			char curr = peek().asciiValue();

			if (!peek().isDigit() && !('a' <= curr && curr <= 'f') &&
			    !('A' <= curr && curr <= 'F')) {
				break;
			}

			next();
		}

		return where_ - 1;
	}

	usize Lexer::numLiteral() {
		bool was_dot = false;
		bool was_e = false;

		if (peek().isAsciiValue('0') && (peek(1).isAsciiValue('b') || peek(1).isAsciiValue('B')))
			return numBinaryLiteral();

		if (peek().isAsciiValue('0') && peek(1).isAsciiValue('x'))
			return numHexLiteral();

		next(); // first char - digit
		while (!peek().isEOF()) {
			if (!peek().isDigit()) {
				if (!was_dot && peek().isAsciiValue('.')) {
					was_dot = true;
				} else if (!was_e && peek().isAsciiValue('e')) {
					was_e = true;
					was_dot = true;
					if (peek(1).isAsciiValue('+') or peek(1).isAsciiValue('-')) {
						next();
					}
				} else {
					// @TODO: perhaps add some errors/skips here
					break;
				}
			}

			next();
		}

		return where_ - 1;
	}

	usize Lexer::string() {
		next();
		while (!peek().isAsciiValue('"')) {
			// @TODO add escaping
			// @TODO add support for formatted string
			next();
		}
		next();
		return where_ - 2;
	}

	Tokens Lexer::parGroup(lexer::Char::ParType end) {
		Tokens out;
		next(); // par open

		while (!peek().isParClose(end)) {
			if (isEOF()) {
				// @TODO: error - unclosed par
				return out;
			}
			parseSingleInto(out);
		}

		next(); // par close
		return out;
	}

	bool Lexer::isEOF() const {
		return where_ >= charArray_.getArray().size();
	}

	bool Lexer::isEOL() const {
		return peek().isAsciiValue('\n');
	}

	bool Lexer::isCommentBegin() const {
		return tryRawValue('/') && tryRawValue('/', 1);
	}

	bool Lexer::isBlockCommentBegin() const {
		return tryRawValue('/') && tryRawValue('*', 1);
	}

	bool Lexer::isBlockCommentEnd() const {
		return tryRawValue('*') && tryRawValue('/', 1);
	}

	bool Lexer::isStringBegin() const {
		return tryRawValue('"');
	}

	void init() {
		static bool was_init = false;
		if (was_init) {
			return;
		}
		// Put inits here
		rift_def::key_spec_op::init();
		was_init = true;
	}

	lexer::TokenData tokenizeFile(const fs::FilePath& file, bool dprint) {
		Lexer lexer(file);
		return {lexer.tokenize(dprint), file.getContent()};
	}

}
