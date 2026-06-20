#include "lexer_class.hpp"

#include <diagnostic_interactive/message.hpp>

#include <logger/logger.hpp>
#include <unicode_classification/classifications.hpp>

namespace lexer {

	using Class = unicode::Classifications;

	class TokenStartError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "token_start_error" };
		}

	public:
		TokenStartError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnclosedCommentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unclosed_comment_error" };
		}

	public:
		UnclosedCommentError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnclosedStringEolError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unclosed_string_eol_error" };
		}

	public:
		UnclosedStringEolError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnclosedStringEofError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unclosed_string_eof_error" };
		}

	public:
		UnclosedStringEofError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnclosedCharEolError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unclosed_char_eol_error" };
		}

	public:
		UnclosedCharEolError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnclosedCharEofError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unclosed_char_eof_error" };
		}

	public:
		UnclosedCharEofError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class EmptyCharError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "empty_char_error" };
		}

	public:
		EmptyCharError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MultiCharacterCharError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "multi_character_char_error" };
		}

	public:
		MultiCharacterCharError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnknownLiteralTypeSpecifierError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unknown_literal_type_specifier_error" };
		}

	public:
		UnknownLiteralTypeSpecifierError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnmatchedBracketError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lexer",
				     .name          = "unmatched_bracket_error" };
		}

	public:
		class EndBlock final: public dia_int::MessageWithCodeFragmentAndCause {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "note",
					     .family        = "lexer",
					     .name          = "end_block_note" };
			}

		public:
			EndBlock(dia::SourcePosition pos, UChar32 closing_bracket):
				  dia_int::MessageWithCodeFragmentAndCause(pos) {
				std::string s;
				icu::UnicodeString(closing_bracket).toUTF8String(s);
				addArgument<dia_int::TextArgument>("closing_bracket", s);
			}
		};

		UnmatchedBracketError(
			dia::SourcePosition start_pos, dia::SourcePosition expected_pos, UChar32 closing_bracket
		):
			  dia_int::MessageWithCodeFragmentAndCause(start_pos) {
			addAttachedMessage(makeBox<EndBlock>(expected_pos, closing_bracket));
		}
	};

	void Lexer::parseCodeblockInto(Tokens& output) {
		constexpr auto STOP_ON_EOF = [](const Lexer& lexer) { return lexer.isEOF(); };
		parseUntil(output, STOP_ON_EOF);
	}

	void Lexer::parseSingleInto(Tokens& output) {
		dia::SourcePosition source_start = currentPosition();
		if (isEOF()) {
			CORE_PANIC("EOF encountered inside parseSingleInto");
		}
		// Look for numeric literals first since they may start with a '.'
		else if (peek().isDigit() || (peek().is('.') && peek(1).isDigit())) {
			if (peek().is('0') && (peek(1).is('b') || peek(1).is('B')))
				binLiteralHandler(output);
			else if (peek().is('0') && (peek(1).is('o') || peek(1).is('O')))
				octLiteralHandler(output);
			else if (peek().is('0') && (peek(1).is('x') || peek(1).is('X')))
				hexLiteralHandler(output);
			else
				decLiteralHandler(output);

		} else if (isBlockCommentBegin()) {
			blockCommentHandler(output);
		} else if (isCommentBegin()) {
			commentHandler(output);
		} else if (isFormatStringBegin()) {
			formatStringHandler(output);
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
		} else {
			if (not peek().is(Class::whitespace))
				logger->log(makeBox<TokenStartError>(source_start));
			next();
		}
	}

	void Lexer::commentHandler(Tokens& output) {
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
		if (keep_comments)
			output.push_back(Token::makeComment(file->getCharRange(begin, end + 1), source_position)
			);
	}

	void Lexer::blockCommentHandler(Tokens& output) {
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
		if (keep_comments)
			output.push_back(Token::makeComment(file->getCharRange(begin, end + 1), source_position)
			);
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

	base::Optional<Token> Lexer::typeSpecifierHandler() {
		if (isEOF() || !peek().is(Class::name_start)) return {};
		usize suffix_begin     = where;
		auto  suffix_start_pos = currentPosition();
		usize lookahead        = 0;

		while (peek(lookahead).is(Class::name_continue)) lookahead++;

		if (lookahead == 0) return {};

		base::RawView suffix_view = file->getCharRange(where, where + lookahead);
		auto specifier = lang_def::strAsNumericLiteralTypeSpecifier(base::StrID(suffix_view));
		if (specifier != lang_def::NumericLiteralTypeSpecifier::NotATypeSpecifier) {
			skip(lookahead);
			usize               suffix_end = where - 1;
			dia::SourcePosition source_position(suffix_start_pos, suffix_end);
			addTokenMsg(suffix_begin, suffix_end, "numLiteralTypeSpecifier");
			return Token::makeTypeSpecifier(
				file->getCharRange(suffix_begin, suffix_end + 1), source_position
			);
		}
		logger->log(makeBox<UnknownLiteralTypeSpecifierError>(suffix_start_pos));
		skip(lookahead);
		return {};
	}

	template<typename NumberParser>
	void Lexer::numericLiteralHandler(Tokens& output, NumberParser parse_number) {
		usize begin        = where;
		auto  source_start = currentPosition();

		parse_number();

		usize               number_end = where - 1;
		dia::SourcePosition number_pos(source_start, number_end);
		auto                value_token
			= Token::makeNumLiteral(file->getCharRange(begin, number_end + 1), number_pos);
		addTokenMsg(begin, number_end, "numLiteral");

		auto opt_type_specifier_token = typeSpecifierHandler();

		usize               full_group_end = where - 1;
		dia::SourcePosition full_pos(source_start, full_group_end);
		auto                full_view = file->getCharRange(begin, full_group_end + 1);
		match_optional(opt_type_specifier_token) {
			opt_some_move(specifier) {
				output.push_back(Token::makeNumLiteralGroup(
					full_view, std::move(value_token), std::move(specifier), full_pos
				));
			}
			opt_none {
				output.push_back(
					Token::makeNumLiteralGroup(full_view, std::move(value_token), full_pos)
				);
			}
		}

		addTokenMsg(begin, full_group_end, "numLiteralGroup");
		return;
	}

	void Lexer::binLiteralHandler(Tokens& output) {
		numericLiteralHandler(output, [&]() {
			skip(2);  // 0b
			while (peek().isBinDigit()) next();
		});
	}

	void Lexer::octLiteralHandler(Tokens& output) {
		numericLiteralHandler(output, [&]() {
			skip(2);  // 0o
			while (peek().isOctDigit()) next();
		});
	}

	void Lexer::hexLiteralHandler(Tokens& output) {
		numericLiteralHandler(output, [&]() {
			skip(2);  // 0x
			while (peek().isHexDigit()) next();
		});
	}

	void Lexer::decLiteralHandler(Tokens& output) {
		numericLiteralHandler(output, [&]() {
			bool was_dot = false;
			bool was_e   = false;
			next();  // first char - digit
			while (!isEOF()) {
				if (!peek().isDigit()) {
					if (!was_dot && peek().is('.')) {  // Only one dot can appear.
						was_dot = true;
					} else if (!was_e && (peek().is('e') || peek().is('E'))) {  // Only one 'e' can
						                                                        // appear.
						was_e   = true;
						was_dot = true;
						next();  // 'e'
					} else {
						break;
					}
				}
				next();
			}
		});
	}

	void Lexer::stringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();
		bool  closed       = true;

		next();
		while (!peek().is('"')) {
			// A backslash as the very last character must not be skipped over,
			// otherwise skip(2) would advance past EOF; let the EOF branch report it.
			if (peek().is('\\') && !peek(1).is(Class::END_OF_FILE_VALUE)) {
				skip(2);
			} else if (isEOL()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				auto                error = makeBox<UnclosedStringEolError>(err_pos);
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
			file->getCharRange(begin + 1, end + 1 - static_cast<usize>(closed)), source_position
		));
	}

	void Lexer::formatSubStringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();

		while (!peek().is('"') && !peek().is('{') && !isEOL() && !isEOF())
			// Do not skip past EOF on a trailing backslash; the loop condition handles EOF.
			if (peek().is('\\') && !peek(1).is(Class::END_OF_FILE_VALUE))
				skip(2);
			else
				next();

		end = where - 1;

		dia::SourcePosition source_position(source_start, end);

		addTokenMsg(begin, end, "format_sub_string");
		output.push_back(
			Token::makeFormatStringSubString(file->getCharRange(begin, end + 1), source_position)
		);
	}

	void Lexer::formatStringHandler(Tokens& output) {
		usize begin = where;
		usize end{};
		auto  source_start = currentPosition();
		bool  closed       = true;

		CORE_DEV_LOG(Lexer, "format string begin", generateLineColumnInfo(), "\n");

		Tokens inner_tokens;

		skip(2);  // skip f"
		while (!peek().is('"')) {
			if (isEOL()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				auto                error = makeBox<UnclosedStringEolError>(err_pos);
				logger->log(std::move(error));
				closed = false;
				break;
			} else if (isEOF()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				logger->log(makeBox<UnclosedStringEofError>(err_pos));
				closed = false;
				break;
			} else if (peek().is('{')) {
				bracketHandler(inner_tokens);
			} else {
				formatSubStringHandler(inner_tokens);
			}
		}
		end = where - 1 + usize(closed);

		if (closed) next();

		dia::SourcePosition source_position(source_start, end);

		Token sentinel_begin = Token::makeSentinel(
			file->getCharRange(begin + 1, begin + 2), { source_start.getLocation(), begin, begin }
		);
		Token sentinel_end = Token::makeSentinel(
			file->getCharRange(end, end + 1), { source_start.getLocation(), end, end }
		);

		addTokenMsg(begin, end, "format_string");
		output.push_back(Token::makeFormatString(
			std::move(inner_tokens),
			std::move(sentinel_begin),
			std::move(sentinel_end),
			source_position
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
			// A backslash as the very last character must not be skipped over,
			// otherwise skip(2) would advance past EOF; let the EOF branch report it.
			if (peek().is('\\') && !peek(1).is(Class::END_OF_FILE_VALUE)) {
				skip(2);
			} else if (isEOL()) {
				dia::SourcePosition err_pos(source_start, where - 1);
				auto                error = makeBox<UnclosedCharEolError>(err_pos);
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

		CORE_DEV_LOG(Lexer, "group begin", generateLineColumnInfo(), "\n");


		Tokens inner_tokens;
		next();  // par open
		constexpr auto IS_GROUP_END = [](const Lexer& lexer) {
			return lexer.isEOF() || lexer.peek().is(Class::close_bracket);
		};
		parseUntil(inner_tokens, IS_GROUP_END);

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
		CORE_DEV_LOG(Lexer, "group end", "\n");
	}
}
