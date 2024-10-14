#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> GeneralPrefix::parse(LangParserState& state, u64 length) {
		std::cerr << "Parsing General Prefix Expressions" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (!state[0].isOperator()) return Lower::parse(state, length);

		auto out = base::make_unique<GeneralPrefix>(state.getPosition(), state[0].getValue());

		state.parse(out).eatOne();
		state.parse(out).with(&out->expr, parse, length - 1);

		return out;
	}
}
