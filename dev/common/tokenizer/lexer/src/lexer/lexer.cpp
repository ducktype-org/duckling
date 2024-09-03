/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include <diagnostic/logger.hpp>
#include <token_file/file.hpp>
#include <duckling_definitions/key_spec_op.hpp>
#include <base/exceptions.hpp>
#include <base/init_guard.hpp>

#include "lexer.hpp"
#include "classifications.hpp"

namespace lexer {
	void init() {
		CORE_SIMPLE_INIT_GUARD_BEGIN
		// Put inits here
		Classifications::init();
		duckling_def::key_spec_op::init();
		CORE_SIMPLE_INIT_GUARD_END
	}

	tokenizer::OwnFile tokenizeFile(const fs::FilePath& path) {
		auto file = tokenizer::makeTokenFile(path);
		file->tokenize();
		if (file->getLogger().bad()) {
			file->getLogger().dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return file;
	}

}
