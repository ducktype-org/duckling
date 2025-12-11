#include "lang_state_unmethods.hpp"

#include "../lang_parser_state.hpp"
#include "hierarchy/expr_holders.hpp"
#include "hierarchy/not_statements/expr_element.hpp"  // IWYU pragma: keep
#include "implementations/preamble.hpp"

namespace pst::internal {
	dia::SourcePosition getPosition(LangParserState& state) { return state.getPosition(); }

	void parseExprIntoHolder(LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun) {
		state.parse(out).with(&out->expr, parse_fun);
	}

	bool isSentinel(LangParserState& state, i64 fwd) {
		return state[fwd].is(lexer::Token::Type::Sentinel);
	}

	u64 streamSize(LangParserState& state) {
		return state.ctokens().size();
	}
}
