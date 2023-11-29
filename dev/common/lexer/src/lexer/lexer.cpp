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
		  file_content(this->file->getContent()) {
			auto result = decode<fs::Encoding::UTF8>(file_content.view(), console);
			if (!result) {
				console.print(std::cerr);
				throw base::LogicError("Error while decoding");
			}
			char_array = std::move(result.value());
		}

	TokenizationResult Lexer::tokenize(bool dprint) {
		tokens.clear();
		token_messages = dprint;
		try {
			codeblock();
		} catch (...) {
			console.print(std::cerr);
			throw;
		}
		if (dprint) {
			// @TODO: better customization of this dprint
			console.print(std::cerr);
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
			console.add(printer::Message(
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
		usize begin, usize end, std::string_view token_type, printer::MessageType message_type
	) {
		if (token_messages) {
			console.add(printer::Message(
				{ { "Add token: " },
			      { std::string(token_type) },
			      { "(" },
			      { std::string(char_array.composeRaw(begin, end).stringView()) },
			      { ")" } },
				message_type
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
		usize          begin = where;
		auto fromEnd = [file = file, line = line, column = column, start = where](u64 end) {
			return SourcePosition(file, line, column, start, end);
		};
		if (isEOF()) {
			// @TODO: error
			RIFT_PANIC("EOF encountered inside parseSingleInto");
		}
		// @TODO: for now comments aren't saved because it's way to hard to parse with the current
		// parser
		else if (isCommentBegin()) {
			usize end = comment(/*output*/);
			auto sourcePosition = fromEnd(end);

			addTokenMsg(begin + 2, end, "line comment", printer::MessageType::DEBUG);
			// output.push_back(Token::makeComment(charArray_.composeRaw(begin + 2, end),
			// source_position));
		} else if (isBlockCommentBegin()) {
			usize end = blockComment();
			auto sourcePosition = fromEnd(end);

			addTokenMsg(begin + 2, end, "block comment", printer::MessageType::DEBUG);
			// output.push_back(Token::makeComment(charArray_.composeRaw(begin + 2, end),
			// source_position));
		} else if (peek().is(Class::operator_start)) {
			usize end = oper();
			auto sourcePosition = fromEnd(end);
			addTokenMsg(begin, end, "operator", printer::MessageType::DEBUG);
			output.push_back(Token::makeOperator(char_array.composeRaw(begin, end), sourcePosition)
			);
		} else if (peek().is(Class::name_start)) {
			usize end = identifier();
			auto sourcePosition = fromEnd(end);
			std::string message;
			output.push_back(
				Token::makeIdentifier(char_array.composeRaw(begin, end), sourcePosition)
			);
			if (output.back().getType() == Token::Type::Identifier)
				addTokenMsg(begin, end, "identifier", printer::MessageType::DEBUG);
			else if (output.back().getType() == Token::Type::Keyword)
				addTokenMsg(begin, end, "keyword", printer::MessageType::DEBUG);
		} else if (isStringBegin()) {
			usize end = string();
			auto sourcePosition = fromEnd(end + 1);

			addTokenMsg(begin + 1, end, "string", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeString(char_array.composeRaw(begin + 1, end), sourcePosition)
			);
		} else if (peek().is(Class::open_bracket)) {
			// the groups are constructed here
			auto group_type = peek().getValue();
			auto group_end = peek().bracketPair();
			if (token_messages)
				console.add({ { { "group begin" } }, printer::MessageType::DEBUG });  // @TODO: better
			Tokens inner_tokens = parGroup(group_end);

			auto sourcePosition = fromEnd(where - 1);

			SourcePosition sentinelPosition(file, line, column, where - 1); 
			auto sentinelView = char_array.composeRaw(where - 1, where -1);
			Token sentinel = Token::makeSentinelEnd(sentinelView, sentinelPosition);

			output.push_back(
				Token::makeGroup(group_type, std::move(inner_tokens), std::move(sentinel), sourcePosition)
			);
			if (token_messages)
				console.add({ { { "group end" } }, printer::MessageType::DEBUG });
		} else if (peek().is(Class::special)) {
			usize end = special();
			auto sourcePosition = fromEnd(end);
			addTokenMsg(begin, end, "special", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeSpecial(char_array.composeRaw(begin, end), sourcePosition)
			);
		} else if (peek().isDigit()) {
			usize end = numLiteral();
			auto sourcePosition = fromEnd(end);

			addTokenMsg(begin, end, "numLiteral", printer::MessageType::DEBUG);
			output.push_back(
				Token::makeNumLiteral(char_array.composeRaw(begin, end), sourcePosition)
			);
		} else {
			if (not peek().is(Class::whitespace)) {
				console.add(printer::Message(
					{
						{ "Skipped" },
						{ generateLineColumnInfo() },
						{ "(" },
						{ std::string(peek().rawStr()) },
						{ ")" },
					},
					printer::MessageType::DEBUG
				));
			}
			next();  // in else??
		}
	}

	// @TODO: think if we want to allow some kind of nested single line comments, the version
	// commented out below doesn't work
	usize Lexer::comment(/*Tokens& output*/) {
		skip(2);  // "//"
		// usize begin = where_;
		// SourcePosition position(file_, lineNumber_, columnNumber_, where_);
		while (true) {
			if (isEOF()) {
				return where - 1;
			} else if (isEOL()) {
				skip(1);
				return where - 2;
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
		skip(2);  // "/*"
		while (true) {
			if (isEOF()) {
				console.add(printer::Message(
					{
						{ "Missing end of block comment at " },
						{ generateLineColumnInfo() },
					},
					printer::MessageType::WARNING
				));
				return where - 1;
			}
			// @FIXME: with the current way of adding tokens this doesn't add them as separate
			// comments just pairs starts and ends
			else if (isBlockCommentBegin()) {
				blockComment();
			} else if (isBlockCommentEnd()) {
				skip(2);
				return where - 3;
			} else {
				next();
			}
		}
	}

	usize Lexer::oper() {
		while (peek().is(Class::operator_continue)) next();
		return where - 1;
	}

	usize Lexer::identifier() {
		next();  // first char - character
		while (peek().is(Class::name_continue)) next();
		return where - 1;
	}

	usize Lexer::special() {
		next();
		return where - 1;
	}

	usize Lexer::numBinaryLiteral() {
		skip(2);  // 0b
		while (peek().isBinDigit()) next();
		return where - 1;
	}

	usize Lexer::numHexLiteral() {
		skip(2);  // 0x
		while (peek().isHexDigit()) next();
		return where - 1;
	}

	usize Lexer::numLiteral() {
		bool was_dot = false;
		bool was_e   = false;

		if (peek().is('0') && (peek(1).is('b') || peek(1).is('B')))
			return numBinaryLiteral();

		if (peek().is('0') && peek(1).is('x')) return numHexLiteral();

		next();  // first char - digit
		while (!peek().is(Class::end_of_file_value)) {
			if (!peek().isDigit()) {
				if (!was_dot && peek().is('.')) {
					was_dot = true;
				} else if (!was_e && peek().is('e')) {
					was_e   = true;
					was_dot = true;
					if (peek(1).is('+') or peek(1).is('-')) next();
				} else {
					// @TODO: perhaps add some errors/skips here
					break;
				}
			}

			next();
		}

		return where - 1;
	}

	usize Lexer::string() {
		next();
		while (!peek().is('"')) {
			if (peek().is('\\')) next();
			next();
		}
		next();
		return where - 2;
	}

	Tokens Lexer::parGroup(UChar32 group_end) {
		Tokens out;
		next();  // par open
		constexpr auto isGroupEnd = [](const Lexer& lexer) {
			return lexer.isEOF() || lexer.peek().is(Class::close_bracket);
		};
		parseUntil(out, isGroupEnd);

		if (peek().is(group_end)) next(); // par close
		else if (isEOF()); // log eof error here
		else; // log unclosed parenthesis error here

		return out;
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
