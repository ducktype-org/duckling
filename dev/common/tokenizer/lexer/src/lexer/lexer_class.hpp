#pragma once

#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <printer/stream_printer.hpp>
#include <token_file/file.hpp>
#include <vector>

#include "char.hpp"
#include "token.hpp"

namespace lexer {

	/**
	 * @brief Class used to manage lexing
	 *
	 * @todo Add format string lexing
	 * @todo Improve unicode support(soon: identifier normalization, at some point: ignorable format
	 * controls)
	 */
	class Lexer {
	public:
		/**
		 * @note if file decoding fails outputs the reason to cerr and throws LogicError
		 */
		explicit Lexer(tokenizer::BorrowFile);

		[[nodiscard]]
		TokenData tokenize();

		[[nodiscard]]
		const dia::Logger& getErrorState() const {
			return errorState;
		}

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
		bool tryRawValue(char rawValue, usize fwd = 0) const;
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
		void specialHandler(Tokens& output);
		void decLiteralHandler(Tokens& output);
		void binLiteralHandler(Tokens& output);
		void hexLiteralHandler(Tokens& output);
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
		/**@}*/

		/**
		 * @return std::string in format `(<line number>:<column number>)`
		 */
		[[nodiscard]]
		std::string generateLineColumnInfo(usize fwd = 0) const;

		[[nodiscard]]
		dia::SourcePosition currentPostion() const;

		usize                 where = 0;  ///< Current position in file
		tokenizer::BorrowFile file;
		dia::Logger&          errorState;
		CharArray&            char_array;
		Tokens                tokens;

		/**
		 * @brief Informs whether to print messages about what tokens are created to the debug
		 * stream based on the PRINT_LOG define
		 */
		static constexpr bool tokenMessages() {
// #ifdef PRINT_LOG
			// return true;
// #else
			return false;
// #endif
		}

		printer::StreamPrinter streamPrinter;

		void addTokenMsg(usize begin, usize end, std::string_view token_type);
	};

}
