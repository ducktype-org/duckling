#include "../../hierarchy/expressions/logic_or.hpp"

#include "../../hierarchy/expressions/logic_and.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicOr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

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
		if (!or_found) return Lower::parse(state, length);

		auto out = makeBox<LogicOr>(pos);

		PARSE().with(&out->left, Lower::parse, +or_fwd);
		PARSE().one(Keyword::Or);
		PARSE().with(&out->right, Self::parse, length - or_fwd - 1);

		PST_RETURN out;
	}
}
