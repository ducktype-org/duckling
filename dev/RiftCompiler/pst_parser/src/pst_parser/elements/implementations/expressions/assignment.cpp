#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Assignment::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Assignment" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		bool found = false;
		u64  place = 0;
		for (u64 i = 0; i < length; i++) {
			if (ExprClassify::isAssignment(state, i)) {
				if (!found) {
					found = true;
					place = i;
				} else {
					std::cerr << "multiple assignments" << std::endl;
					// Multiple assignments in one expression
					fastForward(state, length);
					return nullptr;
				}
			}
		}
		if (!found) return Lower::parse(state, length);
		auto out = base::make_unique<Assignment>(pos);

		state.parse(out).with(&out->variables, Lower::parse, +place);

		out->type = state[0].getValue();
		state.parse(out).eatOne();

		state.parse(out).with(&out->value, Lower::parse, length - place - 1);

		return out;
	}
}
