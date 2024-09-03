/**
 * @file parser.cpp
 * @author Andrzej Radzimiński
 */

#include "parser.hpp"
#include <base/init_guard.hpp>
#include <lexer/lexer.hpp>
#include <token_parser_core/tpc.hpp>

namespace pst {
	void init() {
		CORE_SIMPLE_INIT_GUARD_BEGIN
		tpc::init();
		lexer::init();
		CORE_SIMPLE_INIT_GUARD_END
	}
}
