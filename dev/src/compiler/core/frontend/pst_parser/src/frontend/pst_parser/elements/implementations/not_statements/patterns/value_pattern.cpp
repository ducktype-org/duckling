#include "../../../hierarchy/not_statements/patterns/value_pattern.hpp"

#include "../../../hierarchy/expressions/ternary.hpp"
#include "../preamble.hpp"

namespace pst {
	MBox<ValuePattern> ValuePattern::parse(LangParserState& state) {
		if (!state[0].isNumLiteralGroup() && !state[0].isString() && !state[0].is(Keyword::True)
		    && !state[0].is(Keyword::False)
		    && !state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			return nullptr;
		}

		auto position = state.getPosition();
		auto out      = makeBox<ValuePattern>(position);
		state.parse(out).one(&out->expression);

		return out;
	}

	namespace {
		bool implementsValuePatternEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
			    || state[fwd].is(Special::Comma);
		}
	}

	MBox<ExprElement> ExprParserHelper::parseValuePattern(LangParserState& state) {
		return expr::parseUntil<expr::Ternary, implementsValuePatternEnd>(state);
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

	LangElement::HashAlg& ValuePattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ValuePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitValuePattern(*this);
	}
}
