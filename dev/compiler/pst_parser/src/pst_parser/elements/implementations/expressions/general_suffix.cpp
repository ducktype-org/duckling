#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> GeneralSuffix::parseRecursive(LangParserState& state, i64 length, u64 iter) {
		if (iter == 0) return Lower::parse(state, length);

		auto out = makeBox<GeneralSuffix>(state.getPosition(), state[length - 1].getValue());

		state.parse(out).with(&out->expr, parseRecursive, length - 1, iter - 1);

		state.parse(out).eatOne();

		return out;
	}

	MBox<ExprElement> GeneralSuffix::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing General Suffix Expressions" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		i64 fwd            = 0;
		i64 reduced_length = length;
		// Here this should include the prefix word operators in the future
		while (fwd < length && state[fwd].isOperator()) fwd++;
		while (fwd < reduced_length && state[reduced_length - 1].isOperator()) reduced_length--;
		if (fwd == reduced_length) {}  // Error

		if (fwd + 1 < reduced_length && state[reduced_length - 1].isIdentifier()
		    && !state[reduced_length - 2].is(NamedOperator::Period))
			reduced_length--;
		return parseRecursive(state, length, length - reduced_length);
	}
}
