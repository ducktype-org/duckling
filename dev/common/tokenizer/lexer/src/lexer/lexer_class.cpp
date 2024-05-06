#include "lexer_class.hpp"
#include "classifications.hpp"
#include "decode.hpp"
#include "diagnostic/logger.hpp"

namespace lexer {
	using Class = Classifications;

	Lexer::Lexer(tokenizer::BorrowFile file):
		  file(file),
		  errorState(file->getLogger()),
		  char_array(file->getChars()) {
		if (errorState.bad()) {
			errorState.dumpLog(false, std::cerr);
			throw base::LogicError("Lexer initialized with existing error");
		}
	}

	TokenData Lexer::tokenize() {
		tokens.clear();
		codeblock();
		dia::SourcePosition eof_pos(file, where);
		return { std::move(tokens), Token::makeSentinelEof(eof_pos)};
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
			errorState.failAndLog({ "Tried to skip EOF" });
		}
	}

	void Lexer::skip(usize n) {
		for (usize i = 0; i < n; i++) next();
	}

	bool Lexer::tryRawValue(char rawValue, usize fwd) const {
		return char_array.size() > where + fwd && peek(fwd).is(rawValue);
	}

	const Char& Lexer::peek(usize fwd) const { return char_array.at(where + fwd); }

	std::string Lexer::generateLineColumnInfo() const {
		return "(" + std::to_string(line) + ":" + std::to_string(column) + ")";
	}

	void Lexer::addTokenMsg(usize begin, usize end, std::string_view token_type) {
		if (tokenMessages()) {
			streamPrinter.add(printer::Message(
				{ { "Add token: " },
			      { std::string(token_type) },
			      { "(" },
			      { std::string(file->getCharRange(begin, end).stringView()) },
			      { ")" } },
				printer::MessageType::DEBUG
			));
		}
	}

	void Lexer::codeblock() { parseCodeblockInto(tokens); }

	void Lexer::parseUntil(Tokens& output, const LexerCondition& stop) {
		while (!stop(*this)) parseSingleInto(output);
	}

	void Lexer::parseCodeblockInto(Tokens& output) {
		constexpr auto stopOnEOF = [](const Lexer& lexer) { return lexer.isEOF(); };
		parseUntil(output, stopOnEOF);
	}

	void Lexer::parseSingleInto(Tokens& output) {
		dia::SourcePosition sourceStart(file, where);
		if (isEOF()) {
			RIFT_PANIC("EOF encountered inside parseSingleInto");
		}
		// @TODO: for now comments aren't saved as tokens
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
			bracketHandler(output);
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
			if (not peek().is(Class::whitespace))
				errorState.failAndLog(sourceStart, "unexpected character starting token");
			next();  // in else??
		}
	}

	void Lexer::commentHandler([[maybe_unused]] Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

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

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "line comment");
	}

	void Lexer::blockCommentHandler([[maybe_unused]] Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

		skip(2);  // "/*"
		while (true) {
			if (isEOF()) {
				errorState.failAndLog(sourceStart, "Unclosed block comment starting here");
				end = where - 1;
				break;
			} else if (isBlockCommentEnd()) {
				skip(2);
				end = where - 1;
				break;
			} else {
				next();
			}
		}

		dia::SourcePosition sourcePosition(sourceStart, end);
		addTokenMsg(begin, end, "block comment");
	}

	void Lexer::operatorHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

		while (peek().is(Class::operator_continue)) next();
		end = where - 1;

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "operator");
		output.push_back(Token::makeOperator(file->getCharRange(begin, end), sourcePosition));
	}

	void Lexer::nameHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

		next();  // first char - character
		while (peek().is(Class::name_continue)) next();
		end = where - 1;

		dia::SourcePosition sourcePosition(sourceStart, end);
		std::string         message;
		output.push_back(Token::makeIdentifier(file->getCharRange(begin, end), sourcePosition));
		if (output.back().getType() == Token::Type::Identifier)
			addTokenMsg(begin, end, "identifier");
		else if (output.back().getType() == Token::Type::Keyword)
			addTokenMsg(begin, end, "keyword");
	}

	void Lexer::specialHandler(Tokens& output) {
		usize begin       = where;
		usize end         = where;
		auto  sourceStart = currentPostion();

		next();

		dia::SourcePosition sourcePosition(sourceStart, end);
		addTokenMsg(begin, end, "special");
		output.push_back(Token::makeSpecial(file->getCharRange(begin, end), sourcePosition));
	}

	void Lexer::binLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

		skip(2);  // 0b
		while (peek().isBinDigit()) next();
		end = where - 1;

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end), sourcePosition));
	}

	void Lexer::hexLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

		skip(2);  // 0x
		while (peek().isHexDigit()) next();
		end = where - 1;

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end), sourcePosition));
	}

	void Lexer::decLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();

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

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end), sourcePosition));
	}

	void Lexer::stringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();
		bool  closed      = true;

		next();
		while (!peek().is('"')) {
			if (peek().is('\\')) {
				next();
				next();
			} else if (isEOL()) {
				errorState.failAndLog(
					dia::SourcePosition(sourceStart, where - 1),
					"Expected this string to end before the end of line at: "
						+ generateLineColumnInfo()
				);
				closed = false;
				break;
			} else if (isEOF()) {
				errorState.failAndLog(
					dia::SourcePosition(sourceStart, where - 1),
					"Expected this string to end before the end of file"
				);
				closed = false;
				break;
			} else {
				next();
			}
		}
		end = where - 1 + int(closed);
		if (closed) next();

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "string");
		output.push_back(Token::makeString(
			file->getCharRange(begin + 1, end - usize(closed)), sourcePosition
		));
	}

	void Lexer::bracketHandler(Tokens& output) {
		usize end{};
		auto  sourceStart = currentPostion();

		Token::BracketType bracket_type{ peek().value };
		auto               group_end = peek().bracketPair();
		if (tokenMessages())
			streamPrinter.add({ { { base::strConcat("group begin(", line, ":", column, ")") } },
			                    printer::MessageType::DEBUG });


		Tokens inner_tokens;
		next();  // par open
		constexpr auto isGroupEnd = [](const Lexer& lexer) {
			return lexer.isEOF() || lexer.peek().is(Class::close_bracket);
		};
		parseUntil(inner_tokens, isGroupEnd);

		end = where;


		if (peek().is(group_end))
			next();  // par close
		else if (isEOF()) {
			errorState.failAndLog(
				dia::SourcePosition(sourceStart, where - 1),
				"Expected bracket to be closed before the end of file"
			);
			end = where - 1;
		} else {
			errorState.failAndLog(
				dia::SourcePosition(sourceStart, where - 1),
				base::strConcat(
					"Expected brackets starting here to be closed with: `",
					icu::UnicodeString(group_end),
					"` but encountered `",
					icu::UnicodeString(peek().value),
					"` at position ",
					generateLineColumnInfo(),
					" instead"
				)
			);
			end = where - 1;
		}


		dia::SourcePosition sourcePosition(sourceStart, end);

		dia::SourcePosition sentinelPosition(file, end);
		auto                sentinelView = file->getCharRange(end, end);
		Token               sentinel     = Token::makeSentinelEnd(sentinelView, sentinelPosition);

		output.push_back(Token::makeBracketGroup(
			bracket_type, std::move(inner_tokens), std::move(sentinel), sourcePosition
		));
		if (tokenMessages())
			streamPrinter.add({ { { "group end" } }, printer::MessageType::DEBUG });
	}

	bool Lexer::isEOF() const { return peek().is(Class::end_of_file_value); }

	bool Lexer::isEOL() const { return peek().is(Class::newline); }

	bool Lexer::isCommentBegin() const { return tryRawValue('/') && tryRawValue('/', 1); }

	bool Lexer::isBlockCommentBegin() const { return tryRawValue('/') && tryRawValue('*', 1); }

	bool Lexer::isBlockCommentEnd() const { return tryRawValue('*') && tryRawValue('/', 1); }

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	dia::SourcePosition Lexer::currentPostion() const { return { file, where }; }
}
