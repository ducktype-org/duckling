#include "tpc.hpp"
#include <lexer/lexer.hpp>
#include <base/init_guard.hpp>
#include "token_stream.hpp"

namespace tpc {
	void init() {
		DUCKLING_SIMPLE_INIT_GUARD_BEGIN
		lexer::init();
		DUCKLING_SIMPLE_INIT_GUARD_END
	}
}
