#include "lexer_class.hpp"

#include <logger/logger.hpp>

namespace lexer {
	Lexer::Lexer(Ref<tokenizer::TokenSource> file):
		  file(file),
		  logger(file->getIntLogger()),
		  char_array(file->getChars()) {
		if (logger->hasErrors()) {
			logger->terminalPrint(std::cerr);
			throw base::LogicError("Lexer initialized with existing error");
		}
	}

	TokenData Lexer::tokenize() {
		tokens.clear();
		parseCodeblockInto(tokens);
		dia::SourcePosition eof_pos(file->getLocation(), where);
		dia::SourcePosition bof_pos(file->getLocation(), 0);
		return { std::move(tokens),
			     Token::makeSentinelBof(bof_pos),
			     Token::makeSentinelEof(eof_pos) };
	}
}
