#include "pst_automatic.hpp"
#include "lang_parser_state.hpp"

namespace pst {
	void fallbackLen(LangParserState& state, u64 length) {
		if (state.isSkipping()) {
			state.skipEntry();
			return;
		}
		state.setFallback(length);
	}

	void exitFallback(LangParserState& state) {
		if (state.isSkipping()) {
			if (!state.removeEntry()) {
				return;
			}
		}
		state.exitFallback();
	}
}
