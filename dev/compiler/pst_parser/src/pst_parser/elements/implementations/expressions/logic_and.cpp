#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> LogicAnd::parse(LangParserState& state, u64 length) {
		// std::cerr << "Parsing Logical And" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		bool and_found = false;
		u64  and_fwd   = 0;

		for (u64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::And)) {
				and_found = true;
				and_fwd   = i;
				break;
			}
		}
		if (!and_found) return Lower::parse(state, length);

		auto out = base::make_unique<LogicAnd>(pos);

		state.parse(out).with(&out->left, Lower::parse, +and_fwd);
		state.parse(out).one(Keyword::Or);
		state.parse(out).with(&out->right, Self::parse, length - and_fwd - 1);

		return out;
	}
}
