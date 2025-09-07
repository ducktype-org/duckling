
#pragma once
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a pattern which is an expression interpreted as a value. Either a literal,
	 * block expression or an identifier.
	 */
	class ValuePattern final: public AnalysisPattern {
		NAMED_CHILD(expression, UniversalExprHolder);

	public:
		explicit ValuePattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::ValuePattern;
		}

		~ValuePattern() final = default;

		static MBox<ValuePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Pattern";
		}

		[[nodiscard]] base::Optional<AccessLocked<UniversalExprHolder>> getExpression() const {
			return expression.give();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
