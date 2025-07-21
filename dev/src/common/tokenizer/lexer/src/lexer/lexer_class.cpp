#include "lexer_class.hpp"

#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>
#include <unicode_classification/classifications.hpp>

namespace lexer {
	bool Lexer::token_messages = false;

	void Lexer::setTokenMessages(bool value) { token_messages = value; }

	using Class = unicode::Classifications;

	class TokenStartError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Illegal character at the beginning of a token.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		TokenStartError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnclosedCommentError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unclosed block comment starting here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		UnclosedCommentError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class EolLocationNote final: public dia::NoteWithPosition {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "This end of line.";
		}

	public:
		EolLocationNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
	};

	class UnclosedStringEolError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "String unclosed before end of line.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		using EolNote = EolLocationNote;

		UnclosedStringEolError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnclosedStringEofError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "String unclosed before end of file.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		UnclosedStringEofError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnclosedCharEolError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Char unclosed before end of line.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		using EolNote = EolLocationNote;

		UnclosedCharEolError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnclosedCharEofError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Char unclosed before end of file.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		UnclosedCharEofError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class EmptyCharError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Empty char.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		EmptyCharError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MultiCharacterCharError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Char with multiple characters.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		MultiCharacterCharError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnmatchedBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unmatched Bracket.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Lexer;
		}

		UnmatchedBracketError(
			dia::SourcePosition start_pos, dia::SourcePosition expected_pos, UChar32 closing_bracket
		):
			  dia::Error(start_pos) {
			addNote(makeBox<EndBlock>(expected_pos, closing_bracket));
		}

		class EndBlock final: public dia::NoteWithPosition {
		private:
			UChar32 closing_bracket;

		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				std::string str_bracket{};
				icu::UnicodeString(closing_bracket).toUTF8String(str_bracket);
				return "Expected to be closed with " + str_bracket + ".";
			}

		public:
			EndBlock(dia::SourcePosition pos, UChar32 closing_bracket):
				  dia::NoteWithPosition(pos),
				  closing_bracket(closing_bracket) {}
		};
	};

	Lexer::Lexer(Ref<tokenizer::TokenSource> file):
		  file(file),
		  logger(file->getLogger()),
		  char_array(file->getChars()) {
		if (logger->bad()) {
			logger->dumpLog(false, std::cerr);
			throw base::LogicError("Lexer initialized with existing error");
		}
	}

	TokenData Lexer::tokenize() {
		tokens.clear();
		codeblock();
		dia::SourcePosition eof_pos(file->getLocation(), where);
		dia::SourcePosition bof_pos(file->getLocation(), 0);
		return { std::move(tokens),
			     Token::makeSentinelBof(bof_pos),
			     Token::makeSentinelEof(eof_pos) };
	}

	void Lexer::next() {
		if (!isEOF()) {
			if (isEOL()) {
				// handling of CR+LF as one newline
				if (peek().is(0x0D) && peek(1).is(0x0A)) where++;
				where++;
			} else {
				where++;
			}
		} else {
			throw base::LogicError("Tried to skip EOF");
		}
	}

	void Lexer::skip(usize n) {
		for (usize i = 0; i < n; i++) next();
	}

	bool Lexer::tryRawValue(char raw_value, usize fwd) const {
		return char_array.size() > where + fwd && peek(fwd).is(raw_value);
	}

	const Char& Lexer::peek(usize fwd) const { return char_array.at(where + fwd); }

	std::string Lexer::generateLineColumnInfo(usize fwd) const {
		auto [line, column] = file->getLineColumn(where + fwd);
		return "(" + std::to_string(line) + ":" + std::to_string(column) + ")";
	}

	void Lexer::addTokenMsg(usize begin, usize end, std::string_view token_type) {
		if (token_messages) {
			printer::StreamPrinter::printNL({
				"Add token: ",
				std::string(token_type),
				"(",
				std::string(file->getCharRange(begin, end + 1).stringView()),
				")",
			});
		}
	}

	void Lexer::codeblock() { parseCodeblockInto(tokens); }

	void Lexer::parseUntil(Tokens& output, const LexerCondition& stop) {
		while (!stop(*this)) parseSingleInto(output);
	}

	void Lexer::parseCodeblockInto(Tokens& output) {
		constexpr auto stop_on_eof = [](const Lexer& lexer) { return lexer.isEOF(); };
		parseUntil(output, stop_on_eof);
	}

	void Lexer::parseSingleInto(Tokens& output) {
		dia::SourcePosition source_start = currentPosition();
		if (isEOF()) {
			CORE_PANIC("EOF encountered inside parseSingleInto");
		}
		// @TODO: for now comments aren't saved as tokens
		else if (isBlockCommentBegin()) {
			blockCommentHandler(output);
		} else if (isCommentBegin()) {
			commentHandler(output);
		} else if (peek().is(Class::operator_start)) {
			operatorHandler(output);
		} else if (peek().is(Class::name_start)) {
			nameHandler(output);
		} else if (isStringBegin()) {
			stringHandler(output);
		} else if (isCharBegin()) {
			charHandler(output);
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
				logger->log(makeBox<TokenStartError>(source_start));
			next();
		}
	}

	void Lexer::commentHandler([[maybe_unused]] Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();

		skip(1);  // "#"
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

		dia::SourcePosition source_position(source_start, end);

		addTokenMsg(begin, end, "line comment");
	}

	void Lexer::blockCommentHandler([[maybe_unused]] Tokens& output) {
		usize               begin = where;
		usize               end{};
		auto                source_start = currentPosition();
		dia::SourcePosition opening(source_start, begin + 1);

		skip(2);  // "#{"
		while (true) {
			if (isEOF()) {
				logger->log(makeBox<UnclosedCommentError>(opening));
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

		dia::SourcePosition source_position(source_start, end);
		addTokenMsg(begin, end, "block comment");
	}

	void Lexer::operatorHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();

		while (peek().is(Class::operator_continue)) next();
		end = where - 1;

		dia::SourcePosition source_position(source_start, end);

		addTokenMsg(begin, end, "operator");
		output.push_back(Token::makeOperator(file->getCharRange(begin, end + 1), source_position));
	}

	void Lexer::nameHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();

		next();  // first char - character
		while (peek().is(Class::name_continue)) next();
		end = where - 1;

		dia::SourcePosition source_position(source_start, end);
		std::string         message;
		output.push_back(Token::makeIdentifier(file->getCharRange(begin, end + 1), source_position));
		if (output.back().getType() == Token::Type::Identifier)
			addTokenMsg(begin, end, "identifier");
		else if (output.back().getType() == Token::Type::Keyword)
			addTokenMsg(begin, end, "keyword");
	}

	void Lexer::specialHandler(Tokens& output) {
		usize begin        = where;
		usize end          = where;
		auto  source_start = currentPosition();

		next();

		dia::SourcePosition source_position(source_start, end);
		addTokenMsg(begin, end, "special");
		output.push_back(Token::makeSpecial(file->getCharRange(begin, end + 1), source_position));
	}

	void Lexer::binLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();

		skip(2);  // 0b
		while (peek().isBinDigit()) next();
		end = where - 1;

		dia::SourcePosition source_position(source_start, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), source_position));
	}

	void Lexer::hexLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPosition();

		skip(2);  // 0x
		while (peek().isHexDigit()) next();
		end = where - 1;

		dia::SourcePosition sourcePosition(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), sourcePosition));
	}

	void Lexer::decLiteralHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPosition();

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

		dia::SourcePosition source_position(sourceStart, end);

		addTokenMsg(begin, end, "numLiteral");
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), source_position));
	}

	void Lexer::stringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();
		bool  closed       = true;

		next();
		while (!peek().is('"')) {
			if (peek().is('\\')) {
				skip(2);
			} else if (isEOL()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				dia::SourcePosition eol_pos = currentPosition();
				auto                error   = makeBox<UnclosedStringEolError>(err_pos);
				error->addNote(makeBox<UnclosedStringEolError::EolNote>(eol_pos));
				logger->log(std::move(error));
				closed = false;
				break;
			} else if (isEOF()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				logger->log(makeBox<UnclosedStringEofError>(err_pos));
				closed = false;
				break;
			} else {
				next();
			}
		}
		end = where - 1 + usize(closed);
		if (closed) next();

		dia::SourcePosition source_position(source_start, end);

		addTokenMsg(begin, end, "string");
		output.push_back(Token::makeString(
			file->getCharRange(begin + 1, end + 1 - usize(closed)), source_position
		));
	}

	void Lexer::charHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();
		bool  closed       = true;

		usize count = 0;

		next();
		while (!peek().is('\'')) {
			if (peek().is('\\')) {
				skip(2);
			} else if (isEOL()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				dia::SourcePosition eol_pos = currentPosition();
				auto                error   = makeBox<UnclosedCharEolError>(err_pos);
				error->addNote(makeBox<UnclosedCharEolError::EolNote>(eol_pos));
				logger->log(std::move(error));
				closed = false;
				break;
			} else if (isEOF()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				logger->log(makeBox<UnclosedCharEofError>(err_pos));
				closed = false;
				break;
			} else {
				next();
			}
			count++;
		}
		end = where - 1 + usize(closed);
		dia::SourcePosition source_position(source_start, end);

		if (count == 0)
			logger->log(makeBox<EmptyCharError>(source_position));
		else if (count > 1)
			logger->log(makeBox<MultiCharacterCharError>(source_position));

		if (closed) next();


		addTokenMsg(begin, end, "char");
		output.push_back(
			Token::makeChar(file->getCharRange(begin + 1, end + 1 - usize(closed)), source_position)
		);
	}

	void Lexer::bracketHandler(Tokens& output) {
		usize end{};
		auto  source_start = currentPosition();

		Token::BracketType bracket_type{ peek().value };
		auto               group_end           = peek().bracketPair();
		auto               sentinel_begin_view = file->getCharRange(where, where + 1);
		Token              sentinel_begin = Token::makeSentinel(sentinel_begin_view, source_start);
		if (token_messages)
			printer::StreamPrinter::printNL(base::strConcat("group begin", generateLineColumnInfo())
			);


		Tokens inner_tokens;
		next();  // par open
		constexpr auto is_group_end = [](const Lexer& lexer) {
			return lexer.isEOF() || lexer.peek().is(Class::close_bracket);
		};
		parseUntil(inner_tokens, is_group_end);

		end = where;


		if (peek().is(group_end))
			next();  // par close
		else if (isEOF()) {
			logger->log(makeBox<UnmatchedBracketError>(source_start, currentPosition(), group_end));
			end = where - 1;
		} else {
			logger->log(makeBox<UnmatchedBracketError>(source_start, currentPosition(), group_end));
			end = where - 1;
		}


		dia::SourcePosition source_position(source_start, end);

		dia::SourcePosition sentinel_end_position(file->getLocation(), end);
		auto                sentinel_end_view = file->getCharRange(end, end + 1);
		Token sentinel_end = Token::makeSentinel(sentinel_end_view, sentinel_end_position);

		output.push_back(Token::makeBracketGroup(
			bracket_type,
			std::move(inner_tokens),
			std::move(sentinel_begin),
			std::move(sentinel_end),
			source_position
		));
		if (token_messages) printer::StreamPrinter::printNL("group end");
	}

	bool Lexer::isEOF() const { return peek().is(Class::end_of_file_value); }

	bool Lexer::isEOL() const { return peek().is(Class::newline); }

	bool Lexer::isCommentBegin() const { return tryRawValue('#'); }

	bool Lexer::isBlockCommentBegin() const { return tryRawValue('#') && tryRawValue('{', 1); }

	bool Lexer::isBlockCommentEnd() const { return tryRawValue('}') && tryRawValue('#', 1); }

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	bool Lexer::isCharBegin() const { return tryRawValue('\''); }

	dia::SourcePosition Lexer::currentPosition() const { return { file->getLocation(), where }; }
}
