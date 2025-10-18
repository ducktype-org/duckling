#pragma once

#include "char.hpp"
#include "token.hpp"

#include <diagnostic/logger.hpp>
#include <printer/stream_printer.hpp>
#include <token_source/source.hpp>

namespace lexer {

	/**
	 * @brief Class used to manage lexing
	 *
	 * @todo Add format string lexing
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

		[[nodiscard]]
		const Ref<dia::Logger> getLogger() const {
			return logger;
		}

		/**
		 * @brief Sets value of token_messages flag
		 * that determines if lexer print debug token messages to cerr.
		 */
		static void setTokenMessages(bool value);

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
		void charHandler(Tokens& output);
		void specialHandler(Tokens& output);
		void decLiteralHandler(Tokens& output);
		void binLiteralHandler(Tokens& output);
		void octLiteralHandler(Tokens& output);
		void hexLiteralHandler(Tokens& output);
		/**@}*/

		/**
		 * @brief Consumes a numeric literal type suffix (e.g., i32, f64).
		 */
		void parseNumericLiteralTypeSuffix();
		base::Optional<Token> tryParseNumericLiteralTypeSuffix();

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

		/**
		 * @brief Informs whether to print messages about what tokens are created to the debug
		 * stream based on the PRINT_LOG define
		 */
		static bool token_messages;

		void addTokenMsg(usize begin, usize end, std::string_view token_type);
	};

}
