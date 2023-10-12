#include "tpc.hpp"
#include <lexer/lexer.hpp>
#include "token_stream.hpp"

namespace tpc {
	void init() {
		static bool was_init = false;
		if (was_init) return;
		lexer::init();
		was_init = true;
	}
}
