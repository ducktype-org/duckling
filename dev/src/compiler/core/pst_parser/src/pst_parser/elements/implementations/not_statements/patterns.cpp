#include "../../hierarchy/not_statements/patterns.hpp"

#include "preamble.hpp"

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

	/// Flow pattern ///
	MBox<FlowPattern> FlowPattern::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<FlowPattern>(position);

		state.parse(out).one(&out->pattern);

		if (state.parse(out).tryEat(Keyword::As)) {
			tpc::Identifier temp_ident;
			state.parse(out).one(&temp_ident);
			out->as_identifier = temp_ident;
			// TODOP: Why doesn't this work.
			// state.parse(out).one(&out->as_identifier);
		}

		if (state.parse(out).tryEat(NamedOperator::Colon))
			state.parse(out).one(&out->type_constraint);
		return out;
	}

	void FlowPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "flow",)";
		out << R"("analysis_pattern": )";
		nullAwareDprint(pattern, out);

		if (as_identifier) {
			out << ", ";
			out << R"("as_identifier": )";
			nullAwareDprint(*as_identifier, out);
		}
		if (type_constraint) {
			out << ", ";
			out << R"("type_constraint": )";
			nullAwareDprint(*type_constraint, out);
		}
		out << "}";
	}

	void FlowPattern::acceptVisitor(PstVisitor& visitor) const {
		return visitor.visitFlowPattern(*this);
	}

	/// Analysis pattern ///
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
		// TODOP: isLiteral would be nice.
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

	// TODOP: Think about that.
	void AnalysisPattern::dprint(std::ostream&) const {}

	void AnalysisPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitAnalysisPattern(*this);
	}

	/// Deconstructor pattern ///
	MBox<DeconstructorPattern> DeconstructorPattern::parse(LangParserState& state) {
		if (!state[0].isIdentifier() || !state[1].isBracketGroup(lexer::Token::BracketType::Round))
			return nullptr;

		auto position = state.getPosition();
		auto out      = makeBox<DeconstructorPattern>(position);

		state.parse(out).one(&out->deconstructor_name);
		state.parse(out).one(&out->arguments);
		return out;
	}

	void DeconstructorPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "deconstructor",)";
		out << R"("name": )";
		nullAwareDprint(deconstructor_name, out);
		out << R"(, "arguments": [)";
		nullAwareDprint(arguments, out);
		out << "]}";
	}

	void DeconstructorPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitDeconstructorPattern(*this);
	}

	/// Tuple pattern ///
	MBox<TuplePattern> TuplePattern::parse(LangParserState& state) {
		// TODOP: AssertStmtChoice.
		if (!state[0].isBracketGroup(lexer::Token::BracketType::Round)) return nullptr;
		auto position = state.getPosition();
		auto out      = makeBox<TuplePattern>(position);
		state.parse(out).one(&out->elements);
		return out;
	}

	void TuplePattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "tuple",)";
		out << R"("elements": [)";
		nullAwareDprint(elements, out);
		out << "]}";
	}

	void TuplePattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitTuplePattern(*this);
	}

	/// Wildcard pattern ///
	MBox<WildcardPattern> WildcardPattern::parse(LangParserState& state) {
		if (!state[0].is(Special::Underscore)) return nullptr;

		auto position = state.getPosition();
		auto out      = makeBox<WildcardPattern>(position);
		state.parse(out).eatOne();
		return out;
	}

	void WildcardPattern::dprint(std::ostream& out) const {
		out << R"({ "pattern_type": "wildcard" })";
	}

	void WildcardPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitWildcardPattern(*this);
	}

	/// Binding pattern ///
	MBox<BindingPattern> BindingPattern::parse(LangParserState& state) {
		// TODOP: Assert statement choice.
		if (!state[0].isIdentifier()) return nullptr;
		auto position = state.getPosition();
		auto out      = makeBox<BindingPattern>(position);
		state.parse(out).one(&out->name);
		return out;
	}

	void BindingPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "binding",)";
		out << R"("name": )";
		nullAwareDprint(name, out);
		out << "}";
	}

	void BindingPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitBindingPattern(*this);
	}

	/// Value pattern ///
	MBox<ValuePattern> ValuePattern::parse(LangParserState& state) {
		// TODOP: Assert statement choice.
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
		// TODOP: Fix that.
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
