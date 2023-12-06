/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "classifications.hpp"

#include "lexer.hpp"
#include "lexer_class.hpp"
#include <iostream>
#include <rift_definitions/key_spec_op.hpp>
#include <base/exceptions.hpp>
#include <base/init_guard.hpp>

namespace lexer {
	using Class = Classifications;

	Lexer::Lexer(const fs::FilePath& file):
		  file(std::make_shared<fs::FilePath>(file)),
		  file_content(this->file->getContent()),
		  char_array(std::move(decode<fs::Encoding::UTF8>(file_content.view(), errorState))) {
			if (errorState.fail()) {
				errorState.dumpLog(std::cerr);
				throw base::LogicError("Error while decoding");
			}
		}

	TokenizationResult Lexer::tokenize(bool dprint) {
		tokens.clear();
		token_messages = dprint;
		codeblock();
		if (dprint) {
			// @TODO: better customization of this dprint
			log.print(std::cerr);
		}
		SourcePosition eof_pos(file, line, column, where);
		return { std::move(tokens), Token::makeSentinelEof(eof_pos) };
	}

	void Lexer::next() {
		if (!isEOF()) {
			if (isEOL()) {
				// handling of CR+LF as one newline
				if (peek().is(0x0D) && peek(1).is(0x0A)) where++;
				line++;
				column = 1;
				where++;
			} else {
				column++;
				where++;
			}
		} else {
			errorState.failAndLog(printer::Message(
				{
					{ "Tried to skip EOF" },
				},
				printer::MessageType::ERROR
			));
		}
	}

	void Lexer::skip(usize n) {
		for (usize i = 0; i < n; i++) next();
	}

	bool Lexer::tryRawValue(char rawValue, usize fwd) const {
		return char_array.getArray().size() > where + fwd && peek(fwd).is(rawValue);
	}

	const Char& Lexer::peek(usize fwd) const { return char_array.get(where + fwd); }

	std::string Lexer::generateLineColumnInfo() const {
		return "(" + std::to_string(line) + ":" + std::to_string(column) + ")";
	}

	void Lexer::addTokenMsg(
		usize begin, usize end, std::string_view token_type
	) {
		if (token_messages) {
			log.add(printer::Message(
				{ { "Add token: " },
			      { std::string(token_type) },
			      { "(" },
			      { std::string(char_array.composeRaw(begin, end).stringView()) },
			      { ")" } },
				printer::MessageType::DEBUG
			));
		}
	}

	void Lexer::codeblock() { parseCodeblockInto(tokens); }

	void Lexer::parseUntil(Tokens& output, LexerCondition stop) {
		while(!stop(*this)) parseSingleInto(output);
	}

	void Lexer::parseCodeblockInto(Tokens& output) {
		constexpr auto stopOnEOF = [](const Lexer& lexer){
			return lexer.isEOF();
		};
		parseUntil(output, stopOnEOF);
	}

	void Lexer::parseSingleInto(Tokens& output) {
		SourcePosition sourceStart(file, line, column, where);
		if (isEOF()) {
			// @TODO: error
			RIFT_PANIC("EOF encountered inside parseSingleInto");
		}
		// @TODO: for now comments aren't saved because it's way to hard to parse with the current
		// parser
		else if (isCommentBegin()) {
			commentHandler(output);
		} else if (isBlockCommentBegin()) {
			blockCommentHandler(output);
		} else if (peek().is(Class::operator_start)) {
			operatorHandler(output);
		} else if (peek().is(Class::name_start)) {
			nameHandler(output);
		} else if (isStringBegin()) {
			stringHandler(output);
		} else if (peek().is(Class::open_bracket)) {
			groupHandler(output);
		} else if (peek().is(Class::special)) {
			specialHandler(output);
		} else if (peek().isDigit()) {
			if (peek().is('0') && (peek(1).is('b') || peek(1).is('B')))
				binLiteralHandler(output);
			else if (peek().is('0') && peek(1).is('x')) 
				hexLiteralHandler(output);
			else 
				decLiteralHandler(output);
		} else {
			if (not peek().is(Class::whitespace)) {
				errorState.failAndLog(sourceStart, "unexpected character starting token");
			}
			next();  // in else??
		}
	}

	void Lexer::commentHandler([[maybe_unused]]Tokens& output) {
		usize          begin = where;
		usize          end = where;
		SourcePosition sourceStart(file, line, column, where);

		skip(2);  // "//"
		while (true) {
			if (isEOF()) {
				end = where - 1;
				break;
			} else if (isEOL()) {
				end = where - 1;
				skip(1);
				break;
			} else {
				next();
			}
		}

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "line comment");
		//output.push_back(Token::makeComment(
			//char_array.composeRaw(begin, end),sourcePosition));
	}

	void Lexer::blockCommentHandler([[maybe_unused]]Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		skip(2);  // "/*"
		while (true) {
			if (isEOF()) {
				errorState.failAndLog(sourceStart, "Unclosed block comment starting here");
				end = where - 1;
				break;
			}
			else if (isBlockCommentEnd()) {
				skip(2);
				end = where - 1;
				break;
			} else {
				next();
			}
		}

		SourcePosition sourcePosition(sourceStart, end);
		addTokenMsg(begin, end, "block comment");
		// output.push_back(Token::makeComment(char_array.composeRaw(begin, end),
		// source_position));
	}

	void Lexer::operatorHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		while (peek().is(Class::operator_continue)) next();
		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "operator");
		output.push_back(Token::makeOperator(char_array.composeRaw(begin, end), sourcePosition));
	}

	void Lexer::nameHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		next();  // first char - character
		while (peek().is(Class::name_continue)) next();
		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);
		std::string message;
		output.push_back(
			Token::makeIdentifier(char_array.composeRaw(begin, end), sourcePosition)
		);
		if (output.back().getType() == Token::Type::Identifier)
			addTokenMsg(begin, end, "identifier");
		else if (output.back().getType() == Token::Type::Keyword)
			addTokenMsg(begin, end, "keyword");
	}

	void Lexer::specialHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		next();

		SourcePosition sourcePosition(sourceStart, end);
		addTokenMsg(begin, end, "special");
		output.push_back(
			Token::makeSpecial(char_array.composeRaw(begin, end), sourcePosition)
		);
	}

	void Lexer::binLiteralHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		skip(2);  // 0b
		while (peek().isBinDigit()) next();
		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(
			Token::makeNumLiteral(char_array.composeRaw(begin, end), sourcePosition)
		);
	}

	void Lexer::hexLiteralHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		skip(2);  // 0x
		while (peek().isHexDigit()) next();
		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(
			Token::makeNumLiteral(char_array.composeRaw(begin, end), sourcePosition)
		);
	}

	void Lexer::decLiteralHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		bool was_dot = false;
		bool was_e   = false;
		next();  // first char - digit
		while (!isEOF()) {
			if (!peek().isDigit()) {
				if (!was_dot && peek().is('.')) {
					was_dot = true;
				} else if (!was_e && peek().is('e')) {
					was_e   = true;
					was_dot = true;
					if (peek(1).is('+') or peek(1).is('-')) next();
				} else {
					break;
				}
			}
			next();
		}

		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(
			Token::makeNumLiteral(char_array.composeRaw(begin, end), sourcePosition)
		);
	}

	void Lexer::stringHandler(Tokens& output) {
		usize          begin = where;
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		next();
		while (!peek().is('"')) {
			if (peek().is('\\')) {
				next();
				next();
			} else { // @TODO: handle EOL, EOF
				next();
			}
		}
		end = where;
		next();

		SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "string");
		output.push_back(
			Token::makeString(char_array.composeRaw(begin + 1, end - 1), sourcePosition)
		);
	}

	void Lexer::groupHandler(Tokens& output) {
		usize end = where;
		SourcePosition sourceStart(file, line, column, where);

		auto group_type = peek().getValue();
		auto group_end = peek().bracketPair();
		if (token_messages)
			log.add({ { { base::strConcat("group begin(", line, ":", column, ")") } }, printer::MessageType::DEBUG });


		Tokens inner_tokens;
		next();  // par open
		constexpr auto isGroupEnd = [](const Lexer& lexer) {
			return lexer.isEOF() || lexer.peek().is(Class::close_bracket);
		};
		parseUntil(inner_tokens, isGroupEnd);

		if (peek().is(group_end)) next(); // par close
		else if (isEOF()); // log eof error here
		else; // log unclosed parenthesis error here

		end = where - 1;

		SourcePosition sourcePosition(sourceStart, end);

		SourcePosition sentinelPosition(file, line, column, end); 
		auto sentinelView = char_array.composeRaw(end, end);
		Token sentinel = Token::makeSentinelEnd(sentinelView, sentinelPosition);

		output.push_back(
			Token::makeGroup(group_type, std::move(inner_tokens), std::move(sentinel), sourcePosition)
		);
		if (token_messages)
			log.add({ { { "group end" } }, printer::MessageType::DEBUG });
	}

	bool Lexer::isEOF() const { return peek().is(Class::end_of_file_value); }

	bool Lexer::isEOL() const { return peek().is(Class::newline); }

	bool Lexer::isCommentBegin() const { return tryRawValue('/') && tryRawValue('/', 1); }

	bool Lexer::isBlockCommentBegin() const { return tryRawValue('/') && tryRawValue('*', 1); }

	bool Lexer::isBlockCommentEnd() const { return tryRawValue('*') && tryRawValue('/', 1); }

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	void init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN
		// Put inits here
		Classifications::init();
		rift_def::key_spec_op::init();
		RIFT_SIMPLE_INIT_GUARD_END
	}

	lexer::TokenData tokenizeFile(const fs::FilePath& file, bool dprint) {
		Lexer lexer(file);
		auto [tokens, eof_token] = lexer.tokenize(dprint);
		return { std::move(tokens), std::move(eof_token), file.getContent() };
	}

}
