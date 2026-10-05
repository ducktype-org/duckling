// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
// functions that mirror LangParserState methods that can be used without the full definition

#include "../pst_config.hpp"
#include "../pst_state_forward.hpp"
#include "elements_list.hpp"

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>
#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

namespace pst {
	using ExprParseFun = MBox<ExprElement>(LangParserState&);

	// These are needed to not include parser state definition
	namespace internal {
		dia::SourcePosition getPosition(const LangParserState& state);
		HashType            getContextHash(const LangParserState& state);
		void                parseExprIntoHolder(
						   LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun, u64 length
					   );
		bool               isSentinel(LangParserState& state, i64 fwd);
		const TokenStream& getTokenStream(LangParserState& state);
		u64                streamSize(LangParserState& state);
		bool               isGood(LangParserState& state);
	}
}
