#include "../../hierarchy/expressions/logic_and.hpp"

#include "../../hierarchy/expressions/logic_not.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicAnd::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

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

		auto out = makeBox<LogicAnd>(state);

		PARSE().autoFallbackLen(and_fwd).with(&out->left, Lower::parse);
		PARSE().one(Keyword::And);
		PARSE().with(&out->right, Self::parse);

		PST_RETURN out;
	}
}
