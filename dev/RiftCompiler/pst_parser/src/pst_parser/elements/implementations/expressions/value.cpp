#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> ExprValue::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Value" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(lexer::Token::Type::NumLiteral)) {
			std::cerr << "Value expected" << std::endl;
			// Literal expected error
			fastForward(state, length);
			return nullptr;
		}

		auto out    = base::make_unique<ExprValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			std::cerr << "Bad value length" << std::endl;
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
}
