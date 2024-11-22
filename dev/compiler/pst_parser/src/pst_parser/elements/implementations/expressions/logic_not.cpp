#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> LogicNot::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Logical Not" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(Keyword::Not)) return Lower::parse(state, length);

		auto out = base::make_unique<LogicNot>(pos);

		state.parse(out).one(Keyword::Not);
		state.parse(out).with(&out->expr, Self::parse, length - 1);

		return out;
	}
}
