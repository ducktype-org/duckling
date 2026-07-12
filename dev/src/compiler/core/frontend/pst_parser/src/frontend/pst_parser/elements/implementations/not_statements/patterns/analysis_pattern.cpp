#include "../../../hierarchy/not_statements/patterns/patterns.hpp"
#include "../preamble.hpp"

namespace pst {
	MBox<AnalysisPattern> AnalysisPattern::parse(LangParserState& state) {
		// Wildcard pattern: '_'
		if (state[0].is(Special::Underscore)) return WildcardPattern::parse(state);

		// Tuple pattern: (...)
		if (state[0].isBracketGroup(lexer::Token::BracketType::Round))
			return TuplePattern::parse(state);

		// Deconstructor: '<Ident>(...)'
		if (state[0].isIdentifier() && state[1].isBracketGroup(lexer::Token::BracketType::Round))
			return DeconstructorPattern::parse(state);

		// Literal or expression.
		// '1', 'true', 'false', '{...}'
		if (state[0].isNumLiteralGroup() || state[0].isString() || state[0].is(Keyword::True)
		    || state[0].is(Keyword::False)
		    || state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			return ValuePattern::parse(state);
		}

		// Binding pattern. Identifier which is not a destructor.
		if (state[0].isIdentifier()) return BindingPattern::parse(state);

		state.logInt(makeBox<UnrecognizedPatternInCaseError>(state.getPosition()));
		return nullptr;
	}

	void AnalysisPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitAnalysisPattern(*this);
	}
}
