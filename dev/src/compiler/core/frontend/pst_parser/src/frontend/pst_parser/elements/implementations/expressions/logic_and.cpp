#include "../../hierarchy/expressions/logic_and.hpp"

#include "../../hierarchy/expressions/logic_not.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicAnd::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		u64 length = state.ctokens().size();

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
		if (!and_found) return Lower::parse(state);

		auto out = makeBox<LogicAnd>(pos);

		state.parse(out).autoFallbackLen(and_fwd).with(&out->left, Lower::parse);
		state.parse(out).one(Keyword::And);
		state.parse(out).with(&out->right, Self::parse);

		PST_RETURN out;
	}
}
