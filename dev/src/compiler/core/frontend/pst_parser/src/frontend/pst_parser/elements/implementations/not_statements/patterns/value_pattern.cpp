#include "../../../hierarchy/not_statements/patterns/value_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ValuePattern, expression);

	MBox<ValuePattern> ValuePattern::parse(LangParserState& state) {
		if (!state[0].isNumLiteralGroup() && !state[0].isString() && !state[0].is(Keyword::True)
		    && !state[0].is(Keyword::False)
		    && !state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			return nullptr;
		}

		auto out = makeBox<ValuePattern>(state);
		PARSE().one(&out->expression);

		PST_RETURN out;
	}

	void ValuePattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "value",)";
		out << R"("expression": )";
		nullAwareDprint(expression, out);
		out << "}";
	}

	AccessLocked<ValuePatternExprHolder> ValuePattern::getExpression() const {
		return expression.give();
	}

	HashAlg& ValuePattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ValuePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitValuePattern(*this);
	}
}
