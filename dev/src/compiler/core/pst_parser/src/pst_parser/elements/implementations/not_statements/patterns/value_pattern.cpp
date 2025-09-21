#include "../../../hierarchy/not_statements/patterns/value_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<ValuePattern> ValuePattern::parse(LangParserState& state) {
		if (!state[0].isNumLiteral() && !state[0].isString() && !state[0].is(Keyword::True)
		    && !state[0].is(Keyword::False)
		    && !state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			return nullptr;
		}

		auto position = state.getPosition();
		auto out      = makeBox<ValuePattern>(position);
		state.parse(out).one(&out->expression);

		return out;
	}

	void ValuePattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "value",)";
		out << R"("expression": )";
		nullAwareDprint(expression, out);
		out << "}";
	}

	void ValuePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitValuePattern(*this);
	}
}
