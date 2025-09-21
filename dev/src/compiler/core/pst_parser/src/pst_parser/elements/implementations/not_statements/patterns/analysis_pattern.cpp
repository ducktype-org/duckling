#include "../../../hierarchy/not_statements/patterns/patterns.hpp"
#include "../preamble.hpp"

namespace pst {
	class UnrecognizedPatternInCaseError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unrecognized pattern in case error";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnrecognizedPatternInCaseError(dia::SourcePosition pos): dia::Error(pos) {}
	};

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
		if (state[0].isNumLiteral() || state[0].isString() || state[0].is(Keyword::True)
		    || state[0].is(Keyword::False)
		    || state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			return ValuePattern::parse(state);
		}

		// Binding pattern. Identifier which is not a destructor.
		if (state[0].isIdentifier()) return BindingPattern::parse(state);

		state.log(makeBox<UnrecognizedPatternInCaseError>(state.getPosition()));
		return nullptr;
	}

	void AnalysisPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitAnalysisPattern(*this);
	}
}
