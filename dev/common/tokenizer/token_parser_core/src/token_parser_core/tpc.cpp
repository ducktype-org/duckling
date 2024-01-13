#include "tpc.hpp"
#include <lexer/lexer.hpp>
#include <base/init_guard.hpp>
#include "token_stream.hpp"

namespace tpc {
	void init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN
		lexer::init();
		RIFT_SIMPLE_INIT_GUARD_END
	}
}
