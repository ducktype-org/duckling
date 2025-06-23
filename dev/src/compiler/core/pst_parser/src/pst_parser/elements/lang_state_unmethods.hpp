#pragma once
// functions that mirror LangParserState methods that can be used without the full definition

#include "../pst_state_forward.hpp"
#include "elements_list.hpp"

#include <diagnostic/source_position.hpp>

namespace pst {
	using ExprParseFun = MBox<ExprElement>(LangParserState&);

	// These are needed to not include parser state definition
	namespace detail {
		dia::SourcePosition getPosition(LangParserState& state);
		void parseExprIntoHolder(LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun);
		bool isSentinel(LangParserState& state, i64 fwd);
	}
}
