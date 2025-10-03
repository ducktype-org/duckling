#pragma once
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents the '_' wildcard pattern.
	 */

	class WildcardPattern final: public AnalysisPattern {
	public:
		explicit WildcardPattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::WildcardPattern;
		}

		static MBox<WildcardPattern> parse(LangParserState& state);
		void                         dprint(std::ostream& out) const final;
		~WildcardPattern() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Wildcard Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
