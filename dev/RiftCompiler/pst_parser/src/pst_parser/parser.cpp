/**
 * @file parser.cpp
 * @author Andrzej Radzimiński
 */

#include "parser.hpp"
#include <base/init_guard.hpp>
#include <lexer/lexer.hpp>
#include <token_parser_core/tpc.hpp>

namespace pst {

	PST parse(lexer::TokenData&& td) { return { std::move(td) }; }

	PST parse(const fs::FilePath& path) {
		lexer::TokenData td = lexer::tokenizeFile(path);
		return parse(std::move(td));
	}

	void init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN
		tpc::init();
		lexer::init();
		RIFT_SIMPLE_INIT_GUARD_END
	}
}
