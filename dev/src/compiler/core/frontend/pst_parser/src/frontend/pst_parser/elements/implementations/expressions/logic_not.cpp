#include "../../hierarchy/expressions/logic_not.hpp"

#include "../../hierarchy/expressions/comparison_chain.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicNot::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(Keyword::Not)) return Lower::parse(state);

		auto out = makeBox<LogicNot>(state);

		PARSE().one(Keyword::Not);
		PARSE().with(&out->expr, Self::parse);

		PST_RETURN out;
	}
}
