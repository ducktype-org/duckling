#pragma once
#include "analysis_pattern.hpp"
#include "../../lists/flow_pattern_list.hpp"

namespace pst {
	/**
	 * @brief Represents an Tuple pattern, which contains a list of flow patterns.
	 */
	class TuplePattern final: public AnalysisPattern {
		AccessInternal<FlowPatternList> elements;

	public:
		// explicit TuplePattern(const dia::SourcePosition& position);
		explicit TuplePattern(const dia::SourcePosition& position): AnalysisPattern(position) {
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
