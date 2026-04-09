#pragma once

#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a pattern which is an expression interpreted as a value. Either a literal,
	 * block expression or an identifier.
	 */
	class ValuePattern final: public AnalysisPattern {
		NAMED_CHILD(expression, ValuePatternExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit ValuePattern(const LangParserState& state): AnalysisPattern(state) {
			this->element_kind = ElementKind::ValuePattern;
		}

		~ValuePattern() final = default;

		static MBox<ValuePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Pattern";
		}

		[[nodiscard]]
		AccessLocked<ValuePatternExprHolder> getExpression() const;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
