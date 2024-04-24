/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "classifications.hpp"
#include "lexer.hpp"
#include "lexer_class.hpp"
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

	TokenData tokenizeFile(const fs::FilePath& file) {
		Lexer lexer(file);
		auto  result = lexer.tokenize();
		if (lexer.getErrorState().bad()) {
			lexer.getErrorState().dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return result;
	}

}
