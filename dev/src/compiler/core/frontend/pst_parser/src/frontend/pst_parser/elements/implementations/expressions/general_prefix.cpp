#include "../../hierarchy/expressions/general_prefix.hpp"

#include "../../hierarchy/expressions/chain_expr.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> GeneralPrefix::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (!state[0].isPrefixOperator()) return Lower::parse(state, state.ctokens().size());

		auto out = makeBox<GeneralPrefix>(state.getPosition(), state[0].getValue());

		state.parse(out).eatOne();
		state.parse(out).with(&out->expr, parse);

		PST_RETURN out;
	}
}
