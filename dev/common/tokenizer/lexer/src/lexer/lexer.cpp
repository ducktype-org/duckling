/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <diagnostic/logger.hpp>
#include <token_file/file.hpp>
#include <base/exceptions.hpp>

#include "lexer.hpp"

namespace lexer {
	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		auto file = tokenizer::makeTokenFile(path);
		file->tokenize();
		if (file->getLogger().bad()) {
			file->getLogger().dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return file;
	}

}
