// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lang_state_unmethods.hpp"

#include "../lang_parser_state.hpp"
#include "hierarchy/expr_holders.hpp"
#include "hierarchy/not_statements/expr_element.hpp"  // IWYU pragma: keep
#include "implementations/preamble.hpp"

namespace pst::internal {
	dia::SourcePosition getPosition(LangElementConstructionArgument state) {
		return state.source_position;
	}

	HashType getContextHash(LangElementConstructionArgument state) { return state.context_hash; }

	void parseExprIntoHolder(
		LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun, u64 length
	) {
		PARSE().autoFallbackLen(length).with(&out->expr, parse_fun);
	}

	bool isSentinel(LangParserState& state, i64 fwd) {
		return state[fwd].is(lexer::Token::Type::Sentinel);
	}

	const TokenStream& getTokenStream(LangParserState& state) { return state.ctokens(); }

	u64 streamSize(LangParserState& state) { return state.ctokens().size(); }

	bool isGood(LangParserState& state) { return not state.int_err->hasErrors(); }
}
