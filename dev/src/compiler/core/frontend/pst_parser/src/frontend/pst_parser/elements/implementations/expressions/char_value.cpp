#include "../../hierarchy/expressions/char_value.hpp"

#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> ExprCharValue::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isChar()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadCharValueError>(pos));
			return nullptr;
		}

		auto out = makeBox<ExprCharValue>(state, state[0].getValue());
		PARSE().eatOne();

		if (length > 1) state.logInt(makeBox<MoreThanCharValueError>(pos));

		PST_RETURN out;
	}

	void ExprCharValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("char": ")" << string.value.str() << "\"";

		out << "}";
	}

	HashAlg& ExprCharValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, string);
		return partial_hash;
	}

	void ExprCharValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprCharValue(*this);
	}
}
