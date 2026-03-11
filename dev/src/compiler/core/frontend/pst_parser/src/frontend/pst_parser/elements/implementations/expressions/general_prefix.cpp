#include "../../hierarchy/expressions/general_prefix.hpp"

#include "../../hierarchy/expressions/chain_expr.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> GeneralPrefix::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (!state[0].isPrefixOperator()) return Lower::parse(state);

		auto out = makeBox<GeneralPrefix>(state, state[0].getValue());

		PARSE().eatOne().with(&out->expr, parse);

		PST_RETURN out;
	}
}
