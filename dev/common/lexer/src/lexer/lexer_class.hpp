#pragma once

#include <printer/printer.hpp>
#include <filesystem/file.hpp>
#include <vector>
#include "char.hpp"
#include "token.hpp"

namespace lexer {

	struct TokenizationResult {
		Tokens tokens;
		Token  eof_token;
	};

	/**
	 * @brief Class used to manage lexing
	 * 
	 * @todo Improve error handling in lexing, possibly using Error tokens and logging some sensible errors
	 * @todo Add format string lexing and escape handling to string lexing(some sort of parity of back-slashes or something similar should suffice)
	 * @todo Improve comment lexing
	 * @todo Change `name_` member names to a different method of naming 
	 * @todo Improve unicode support(soon: EOLs, vertical spaces, identifier normalization, at some point: ignorable format controls)
	 * @todo Try to improve #parseSingleInto() to be more readable, maybe divide it into some logical parts
	 */
	class Lexer {
	public:
		/**
		 * @note if file decoding fails outputs the reason to cerr and throws LogicError
		 */
		explicit Lexer(const fs::FilePath& file);

		[[nodiscard]]
		TokenizationResult tokenize(bool dprint);

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
		 * @name top level parsers:
		 * @{
		 */
		void codeblock();
		void parseCodeblockInto(Tokens& output);
		void parseSingleInto(Tokens& output);
		/**@}*/

		/** 
		 * @name non-terminal tokens parsers:
		 * @{
		 */
		Tokens parGroup(UChar32 group_end);
		/**@}*/

		/** 
		 * @name terminal tokens parsers:
		 * @{
		 */
		usize comment(/*Tokens& output*/);
		usize blockComment();
		usize oper();
		usize identifier();
		usize special();
		usize numLiteral();
		usize numBinaryLiteral();
		usize numHexLiteral();
		usize string();
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
		std::string generateLineColumnInfo() const;

		/**
		 * @name current position of lexing
		 * @{
		 */
		usize                         where_        = 0;
		usize                         lineNumber_   = 1;
		usize                         columnNumber_ = 1;
		/**@}*/
		std::shared_ptr<fs::FilePath> file_;
		fs::FileContent               fileContent_;
		CharArray                     charArray_;
		Tokens                        tokens_;

		bool token_messages = false; ///< Informs whether to print messages about what tokens are created to the debug stream

		printer::Console console;

		void addTokenMsg(
			usize begin, usize end, std::string_view token_type, printer::MessageType message_type
		);
	};

}
