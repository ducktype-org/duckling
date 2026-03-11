#include "../../hierarchy/expressions/logic_or.hpp"

#include "../../hierarchy/expressions/logic_and.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicOr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		u64 length = state.ctokens().size();

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());


		bool or_found = false;
		i64  or_fwd   = 0;

		for (i64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::Or)) {
				or_found = true;
				or_fwd   = i;
				break;
			}
		}
		if (!or_found) return Lower::parse(state);

		auto out = makeBox<LogicOr>(state);

		state.parse(out).autoFallbackLen(or_fwd).with(&out->left, Lower::parse);
		state.parse(out).one(Keyword::Or);
		state.parse(out).with(&out->right, Self::parse);

		PST_RETURN out;
	}
}
