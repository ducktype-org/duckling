#include "tpc.hpp"

#include "token_stream.hpp"

#include <lexer/lexer.hpp>

namespace tpc {
	void init() {
		static bool was_init = false;
		if (was_init) return;
		tokenStreamInit();
		lexer::init();
		was_init = true;
	}
}
