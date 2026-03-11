#include "../../hierarchy/expressions/numeric_value.hpp"
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> ExprNumericValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(lexer::Token::Type::NumLiteralGroup)) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprNumericValue>(state);
		state.parse(out).one(&out->value);

		if (length > 1) {
			state.logInt(makeBox<MoreThanValueError>(pos));
			fastForward(state, length);
		}

		PST_RETURN out;
	}

	void ExprNumericValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("number": ")" << value.value.str() << "\"";

		if (value.type_specifier.has_value())
			out << R"(, "type_specifier": ")" << value.type_specifier->str() << "\"";

		out << "}";
	}

	HashAlg& ExprNumericValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, value);
		return partial_hash;
	}

	void ExprNumericValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprNumericValue(*this);
	}
}
