// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "pst.hpp"

#include "lang_parser_state.hpp"

namespace pst::internal {
	Box<LangParserState> makeState(
		tpc::TokenStream&& token_stream, Box<LangParserContext>&& ctx, Ref<dia::Logger> int_logger
	) {
		return base::makeBox<LangParserState>(std::move(token_stream), std::move(ctx), int_logger);
	}

	void finalizeParsing(Ref<LangParserState> state) { state->finalize(); }
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(pst::LangParserState)
