#include "../../hierarchy/expressions/logic_and.hpp"

#include "../../hierarchy/expressions/logic_not.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicAnd::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		bool and_found = false;
		i64  and_fwd   = 0;

		for (i64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::And)) {
				and_found = true;
				and_fwd   = i;
				break;
			}
		}
		if (!and_found) return Lower::parse(state, length);

		auto out = makeBox<LogicAnd>(state);

		state.parse(out).with(&out->left, Lower::parse, +and_fwd);
		state.parse(out).one(Keyword::And);
		state.parse(out).with(&out->right, Self::parse, length - and_fwd - 1);

		PST_RETURN out;
	}
}
