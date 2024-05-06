/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "classifications.hpp"
#include "diagnostic/logger.hpp"
#include "lexer.hpp"
#include "lexer_class.hpp"
#include <token_file/file.hpp>
#include <rift_definitions/key_spec_op.hpp>
#include <base/exceptions.hpp>
#include <base/init_guard.hpp>

namespace lexer {
	void init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN
		// Put inits here
		Classifications::init();
		rift_def::key_spec_op::init();
		RIFT_SIMPLE_INIT_GUARD_END
	}

	tokenizer::OwnFile tokenizeFile(const fs::FilePath& path) {
		// @TODO
		auto file = tokenizer::makeTokenFile(path);
		file->tokenize();
		if (file->getLogger().bad()) {
			file->getLogger().dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return file;
	}

}
