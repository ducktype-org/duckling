#pragma once
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents the '_' wildcard pattern.
	 */

	class WildcardPattern final: public AnalysisPattern {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(WildcardPattern, AnalysisPattern);
	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit WildcardPattern(const LangParserState& state): AnalysisPattern(state) {
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
