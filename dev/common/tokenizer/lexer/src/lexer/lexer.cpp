/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "lexer.hpp"

#include "classifications.hpp"

#include <base/exceptions.hpp>
#include <base/init_guard.hpp>
#include <diagnostic/logger.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <token_file/file.hpp>

namespace lexer {
	void init() {
		SIMPLE_INIT_GUARD_BEGIN
		// Put inits here
		Classifications::init();
		lang_def::key_spec_op::init();
		SIMPLE_INIT_GUARD_END
	}

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
