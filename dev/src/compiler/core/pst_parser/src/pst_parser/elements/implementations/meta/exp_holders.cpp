#include "../../hierarchy/expr_holders.hpp"
#include "preamble.hpp"

namespace pst::detail {
	dia::SourcePosition getPosition(LangParserState& state) { return state.getPosition(); }

	void parseExprIntoHolder(LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun) {
		state.parse(out).with(&out->expr, parse_fun);
	}
}
