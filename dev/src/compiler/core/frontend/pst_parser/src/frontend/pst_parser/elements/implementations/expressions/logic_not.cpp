#include "../../hierarchy/expressions/logic_not.hpp"

#include "../../hierarchy/expressions/comparison_chain.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicNot::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(Keyword::Not)) return Lower::parse(state, length);

		auto out = makeBox<LogicNot>(pos);

		PARSE().one(Keyword::Not);
		PARSE().with(&out->expr, Self::parse, length - 1);

		PST_RETURN out;
	}
}
