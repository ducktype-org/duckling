#pragma once
// functions that mirror LangParserState methods that can be used without the full definition

#include "../lang_parser_element_construction_data.hpp"
#include "../pst_config.hpp"
#include "../pst_state_forward.hpp"
#include "elements_list.hpp"

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>
#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

namespace pst {
	using ExprParseFun = MBox<ExprElement>(LangParserState&);

	using LangElementConstructionArgument = LangParserElementConstructionData;

	// These are needed to not include parser state definition
	namespace internal {
		dia::SourcePosition getPosition(LangElementConstructionArgument state);
		HashType            getContextHash(LangElementConstructionArgument state);
		void                parseExprIntoHolder(
						   LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun, u64 length
					   );
		bool               isSentinel(LangParserState& state, i64 fwd);
		const TokenStream& getTokenStream(LangParserState& state);
		u64                streamSize(LangParserState& state);
		bool               isGood(LangParserState& state);
	}
}
