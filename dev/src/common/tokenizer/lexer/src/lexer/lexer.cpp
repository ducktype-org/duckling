/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "lexer.hpp"

#include <base/except/exceptions.hpp>

#include <diagnostic/logger.hpp>
#include <token_source/source.hpp>

namespace lexer {
	Box<tokenizer::TokenSource> tokenizeFile(const fs::File& path) {
		auto file = tokenizer::makeTokenSource(path);
		file->tokenize();
		if (file->getLogger()->bad()) {
			file->getLogger()->dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return file;
	}
}
