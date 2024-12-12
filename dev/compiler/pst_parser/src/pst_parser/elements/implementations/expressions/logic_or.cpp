#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> LogicOr::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Logical Or" << std::endl;
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

		state.parse(out).with(&out->left, Lower::parse, +or_fwd);
		state.parse(out).one(Keyword::Or);
		state.parse(out).with(&out->right, Self::parse, length - or_fwd - 1);

		return out;
	}
}
