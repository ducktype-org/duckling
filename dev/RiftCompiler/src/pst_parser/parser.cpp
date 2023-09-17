/**
 * @file parser.cpp
 * @author Andrzej Radzimiński
 */

#include "parser.hpp"
#include <lexer/lexer.hpp>
#include <token_parser_core/tpc.hpp>

namespace pst {

	PST parse(lexer::TokenData&& td) {
		return PST(std::forward<lexer::TokenData>(td));
	}

	PST parse(const fs::FilePath& path) {
		lexer::TokenData td = lexer::tokenizeFile(path, false);
		return parse(std::move(td));
	}

	void init() {
		static bool was_init = false;
		if (was_init) {
			return;
		}
		tpc::init();
		lexer::init();
		was_init = true;
	}
}
