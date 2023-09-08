#pragma once

#include <printer/printer.hpp>
#include <filesystem/file.hpp>
#include <vector>
#include "char.hpp"
#include "token.hpp"

namespace lexer {
	class Lexer {
	public:
		explicit Lexer(const fs::FilePath &file);

		[[nodiscard]] Tokens tokenize(bool dprint);

	private:
		void next();
		void skip(usize n);
		[[nodiscard]]
		const Char& peek(usize fwd = 0) const;
		[[nodiscard]]
		bool tryRawValue(char rawValue, usize fwd = 0) const;

		// top level parsers:
		void codeblock();
		void parseCodeblockInto(Tokens& output);
		void parseSingleInto(Tokens& output);

		// non-terminal tokens parsers:
		Tokens parGroup(lexer::Char::ParType end);

		// terminal tokens parsers:
		usize comment(/*Tokens& output*/);
		usize blockComment();
		usize oper();
		usize identifier();
		usize special();
		usize numLiteral();
		usize numBinaryLiteral();
		usize numHexLiteral();
		usize string();

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

		[[nodiscard]] std::string generateLineColumnInfo() const;

		usize where_ = 0;
		usize lineNumber_ = 1;
		usize columnNumber_ = 1;
		std::shared_ptr<fs::FilePath> file_;
		CharArray charArray_;
		Tokens tokens_;

		bool token_messages = false;

		printer::Console console;

		void addTokenMsg(usize begin, usize end,
		                 std::string_view token_type,
		                 printer::MessageType message_type);
	};

}
