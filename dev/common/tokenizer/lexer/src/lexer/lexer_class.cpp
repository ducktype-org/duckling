#include <base/unique_pointer.hpp>
#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>

#include "classifications.hpp"
#include "lexer_class.hpp"

namespace lexer {
	using Class = Classifications;

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

		UnclosedStringEolError(dia::SourcePosition pos): dia::Error(pos) {}

		class EolLocationNote final: public dia::NoteWithPosition {
		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				return "This end of line.";
			}

		public:
			EolLocationNote(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
		};
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
			addNote(base::make_unique<EndBlock>(expected_pos, closing_bracket));
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
		dia::SourcePosition bof_pos(file, 0);
		return { std::move(tokens), Token::makeSentinelBof(bof_pos), Token::makeSentinelEof(eof_pos) };
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

	bool Lexer::tryRawValue(char rawValue, usize fwd) const {
		return char_array.size() > where + fwd && peek(fwd).is(rawValue);
	}

	const Char& Lexer::peek(usize fwd) const { return char_array.at(where + fwd); }

	std::string Lexer::generateLineColumnInfo(usize fwd) const {
		auto [line, column] = file->getLineColumn(where + fwd);
		return "(" + std::to_string(line) + ":" + std::to_string(column) + ")";
	}

	void Lexer::addTokenMsg(usize begin, usize end, std::string_view token_type) {
		if (tokenMessages()) {
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
				errorState.log(base::make_unique<TokenStartError>(sourceStart));
			next();
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
		usize               begin = where;
		usize               end{};
		auto                sourceStart = currentPostion();
		dia::SourcePosition opening(sourceStart, begin + 1);

		skip(2);  // "/*"
		while (true) {
			if (isEOF()) {
				errorState.log(base::make_unique<UnclosedCommentError>(opening));
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
		output.push_back(Token::makeOperator(file->getCharRange(begin, end + 1), sourcePosition));
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
		output.push_back(Token::makeIdentifier(file->getCharRange(begin, end + 1), sourcePosition));
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
		output.push_back(Token::makeSpecial(file->getCharRange(begin, end + 1), sourcePosition));
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
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), sourcePosition));
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
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), sourcePosition));
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
		output.push_back(Token::makeNumLiteral(file->getCharRange(begin, end + 1), sourcePosition));
	}

	void Lexer::stringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  sourceStart = currentPostion();
		bool  closed      = true;

		next();
		while (!peek().is('"')) {
			if (peek().is('\\')) {
				skip(2);
			} else if (isEOL()) {
				dia::SourcePosition errPos(sourceStart, where - 1);
				dia::SourcePosition eolPos = currentPostion();
				auto                error  = base::make_unique<UnclosedStringEolError>(errPos);
				error->addNote(base::make_unique<UnclosedStringEolError::EolLocationNote>(eolPos));
				errorState.log(std::move(error));
				closed = false;
				break;
			} else if (isEOF()) {
				dia::SourcePosition errPos(sourceStart, where - 1);
				errorState.log(base::make_unique<UnclosedStringEofError>(errPos));
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
			file->getCharRange(begin + 1, end + 1 - usize(closed)), sourcePosition
		));
	}

	void Lexer::bracketHandler(Tokens& output) {
		usize end{};
		auto  source_start = currentPostion();

		Token::BracketType bracket_type{ peek().value };
		auto               group_end = peek().bracketPair();
		auto sentinel_begin_view = file->getCharRange(where, where + 1);
		Token               sentinel_begin     = Token::makeSentinel(sentinel_begin_view, source_start);
		if (tokenMessages())
			printer::StreamPrinter::printNL(base::strConcat("group begin", generateLineColumnInfo())
			);


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
			errorState.log(
				base::make_unique<UnmatchedBracketError>(source_start, currentPostion(), group_end)
			);
			end = where - 1;
		} else {
			errorState.log(
				base::make_unique<UnmatchedBracketError>(source_start, currentPostion(), group_end)
			);
			end = where - 1;
		}


		dia::SourcePosition source_position(source_start, end);

		dia::SourcePosition sentinel_end_position(file, end);
		auto                sentinel_end_view = file->getCharRange(end, end + 1);
		Token               sentinel_end     = Token::makeSentinel(sentinel_end_view, sentinel_end_position);

		output.push_back(Token::makeBracketGroup(
			bracket_type, std::move(inner_tokens), std::move(sentinel_begin), std::move(sentinel_end), source_position
		));
		if (tokenMessages()) printer::StreamPrinter::printNL("group end");
	}

	bool Lexer::isEOF() const { return peek().is(Class::end_of_file_value); }

	bool Lexer::isEOL() const { return peek().is(Class::newline); }

	bool Lexer::isCommentBegin() const { return tryRawValue('/') && tryRawValue('/', 1); }

	bool Lexer::isBlockCommentBegin() const { return tryRawValue('/') && tryRawValue('*', 1); }

	bool Lexer::isBlockCommentEnd() const { return tryRawValue('*') && tryRawValue('/', 1); }

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	dia::SourcePosition Lexer::currentPostion() const { return { file, where }; }
}
