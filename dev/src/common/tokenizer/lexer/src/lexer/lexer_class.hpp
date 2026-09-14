#pragma once

#include "char.hpp"
#include "token.hpp"

#include <diagnostic/logger.hpp>
#include <printer/stream_printer.hpp>
#include <token_source/source.hpp>

namespace lexer {

	/**
	 * @brief Class used to manage lexing.
	 *
	 * Takes a reference to a source in constructor for ease of use, it only uses the logger and
	 * decoding data.
	 *
	 * Usage: Construct using the TokenSource then use tokenize() to get back the token data. It is
	 * meant to only be used by TokenSource.
	 *
	 * @todo Improve unicode support(soon: identifier normalization, at some point: ignorable format
	 * controls)
	 */
	class Lexer final {
	public:
		/**
		 * @note if file decoding fails outputs the reason to cerr and throws LogicError
		 */
		explicit Lexer(Ref<tokenizer::TokenSource>);

		[[nodiscard]]
		TokenData tokenize();

	private:
		/**
		 * @name CharArray operations
		 * @{
		 */
		void next();
		void skip(usize n);
		[[nodiscard]]
		const Char& peek(usize fwd = 0) const;
		[[nodiscard]]
		bool tryRawValue(char raw_value, usize fwd = 0) const;
		/**@}*/

		/**
		 * @name Top level parsers:
		 * @{
		 */
		using LexerCondition = std::function<bool(const Lexer&)>;
		void parseUntil(Tokens& output, const LexerCondition&);
		void codeblock();
		void parseCodeblockInto(Tokens& output);
		void parseSingleInto(Tokens& output);
		/**@}*/

		/**
		 * @name tokens parsers:
		 * @{
		 */
		void bracketHandler(Tokens& output);
		void commentHandler(Tokens& output);
		void blockCommentHandler(Tokens& output);
		void operatorHandler(Tokens& output);
		void nameHandler(Tokens& output);
		void stringHandler(Tokens& output);
		void formatStringHandler(Tokens& output);
		void formatSubStringHandler(Tokens& output);
		void charHandler(Tokens& output);
		void specialHandler(Tokens& output);

		template<typename NumberParser>
		void numericLiteralHandler(Tokens& output, NumberParser parse_number);

		void decLiteralHandler(Tokens& output);
		void binLiteralHandler(Tokens& output);
		void octLiteralHandler(Tokens& output);
		void hexLiteralHandler(Tokens& output);

		/**
		 * @brief Consumes a numeric literal type suffix (e.g., i32, f64).
		 * @return The suffix token or an empty optional if no suffix exists.
		 */
		base::Optional<Token> typeSpecifierHandler();
		/**@}*/


		/**
		 * @name helper functions checking for patterns ahead
		 * @{
		 */
		[[nodiscard]]
		bool isEOF() const;
		[[nodiscard]]
		bool isEOL() const;
		[[nodiscard]]
		bool isCommentBegin() const;
		[[nodiscard]]
		bool isBlockCommentBegin() const;
		[[nodiscard]]
		bool isBlockCommentEnd() const;
		[[nodiscard]]
		bool isStringBegin() const;
		[[nodiscard]]
		bool isFormatStringBegin() const;
		[[nodiscard]]
		bool isCharBegin() const;
		/**@}*/

		/**
		 * @return std::string in format `(<line number>:<column number>)`
		 */
		[[nodiscard]]
		std::string generateLineColumnInfo(usize fwd = 0) const;

		[[nodiscard]]
		dia::SourcePosition currentPosition() const;

		usize                       where = 0;  ///< Current position in file
		Ref<tokenizer::TokenSource> file;
		Ref<dia::Logger>            logger;
		const CharArray&            char_array;
		Tokens                      tokens;

		void addTokenMsg(usize begin, usize end, std::string_view token_type);
	};

}
