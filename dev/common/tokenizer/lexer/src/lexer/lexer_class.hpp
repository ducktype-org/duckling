#pragma once

#include <printer/printer.hpp>
#include <filesystem/file.hpp>
#include <diagnostic/error_state.hpp>
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
	 * @todo Add format string lexing
	 * @todo Improve unicode support(soon: identifier normalization, at some point: ignorable format controls)
	 */
	class Lexer {
	public:
		/**
		 * @note if file decoding fails outputs the reason to cerr and throws LogicError
		 */
		explicit Lexer(const fs::FilePath& file);

		[[nodiscard]]
		TokenizationResult tokenize();

		[[nodiscard]]
		const dia::ErrorState& getErrorState() const {
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
		void parseUntil(Tokens& output, LexerCondition);
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
		std::string generateLineColumnInfo() const;

		[[nodiscard]]
		dia::SourcePosition currentPostion() const;

		usize                         where        = 0; ///< Current position in file
		usize                         line   = 1;
		usize                         column = 1;
		std::shared_ptr<fs::FilePath> file;
		fs::FileContent               file_content;
		CharArray                     char_array;
		Tokens                        tokens;

		/**
		 * @brief Informs whether to print messages about what tokens are created to the debug stream based on the PRINT_LOG define
		 */
		static constexpr bool token_messages() {
			#ifdef PRINT_LOG
			return true;
			#else
			return false;
			#endif
		}

		dia::ErrorState errorState;
		printer::Console log;

		void addTokenMsg(usize begin, usize end, std::string_view token_type);
	};

}
