#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> ExprValue::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Value" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(lexer::Token::Type::NumLiteral)) {
			std::cerr << "Value expected\n";
			// Literal expected error
			fastForward(state, length);
			return nullptr;
		}

		auto out = box<ExprValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			std::cerr << "Bad value length\n";
			// Bad value length error
			fastForward(state, length - 1);
		}

		return out;
	}

	void ExprValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("number": ")" << number.str() << "\"";

		out << "}";
	}

	void ExprValue::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitExprValue(*this); }
}
