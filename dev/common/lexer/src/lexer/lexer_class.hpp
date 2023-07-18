#pragma once

#include <printer/printer.hpp>
#include <filesystem/file.hpp>
#include <vector>
#include "char.hpp"
#include "token.hpp"

namespace lexer {
	class Lexer {
		public:
			explicit Lexer(CharArray& chars);
			
			[[nodiscard]]
			Tokens tokenize(bool dprint);

		private:
			void next();
			void skip(std::size_t n);
			[[nodiscard]]
			const Char& peek(std::size_t fwd = 0) const;
			[[nodiscard]]
			bool tryRawValue(char rawValue, std::size_t fwd = 0) const;
			
			// top level parsers:
			void codeblock();
			void parseCodeblockInto(Tokens& output);
			void parseSingleInto(Tokens& output);

			// non terminal tokens parsers:
			Tokens parGroup(lexer::Char::ParType end);
			
			// terminal tokens parsers:
			std::size_t comment(/*Tokens& output*/);
			std::size_t blockComment();
			std::size_t oper();
			std::size_t identifier();
			std::size_t special();
			std::size_t numLiteral();
			std::size_t numBinaryLiteral();
			std::size_t numHexLiteral();
			std::size_t string();
			
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
			
			std::string generateLineColumnInfo() const;
			
			std::size_t where_ = 0;
			std::size_t lineNumber_ = 1;
			std::size_t columnNumber_ = 1;
			CharArray& charArray_;
			Tokens tokens_;

			bool token_messages = false;
			
			printer::Console console;
			
			void addTokenMsg(size_t begin, size_t end,
			                 std::string_view token_type, 
			                 printer::MessageType message_type);
	};

}
