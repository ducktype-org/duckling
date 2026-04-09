#pragma once
#include "../../lists/flow_pattern_list.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents an Tuple pattern, which contains a list of flow patterns.
	 */
	class TuplePattern final: public AnalysisPattern {
		NAMED_CHILD(elements, FlowPatternList);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		// explicit TuplePattern(const dia::SourcePosition& position);
		explicit TuplePattern(const LangParserState& state): AnalysisPattern(state) {
			this->element_kind = ElementKind::TuplePattern;
		}

		~TuplePattern() final = default;

		static MBox<TuplePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;

		[[nodiscard]] const AccessLocked<FlowPatternList> getElements() const {
			return elements.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Tuple Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
