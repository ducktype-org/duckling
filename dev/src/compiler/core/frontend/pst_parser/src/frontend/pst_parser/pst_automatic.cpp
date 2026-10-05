// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
			if (!state.removeEntry()) return;
		}
		state.exitFallback();
	}

	void setSoftFallback(LangParserState& state, TokenStreamCondition fun) {
		if (state.isSkipping()) {
			state.skipEntry();
			return;
		}
		state.setSoftFallback(fun);
	}

	void exitSoftFallback(LangParserState& state) {
		if (state.isSkipping()) {
			if (!state.removeEntry()) return;
		}
		state.exitSoftFallback();
	}
}
