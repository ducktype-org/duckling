#include "tpc.hpp"

#include "token_stream.hpp"

#include <base/init_guard.hpp>
#include <lexer/lexer.hpp>

namespace tpc {
	void init() {
		SIMPLE_INIT_GUARD_BEGIN
		lexer::init();
		SIMPLE_INIT_GUARD_END
	}
}
