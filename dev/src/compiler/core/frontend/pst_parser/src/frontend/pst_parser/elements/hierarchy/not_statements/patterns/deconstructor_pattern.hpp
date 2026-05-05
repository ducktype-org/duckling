#pragma once
#include "../../lists/flow_pattern_list.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a deconstructor pattern.
	 */
	class DeconstructorPattern final: public AnalysisPattern {
		NAMED_CHILD(deconstructor_name, IdentifierWrapper);
		NAMED_CHILD(arguments, FlowPatternList);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit DeconstructorPattern(const LangParserState& state): AnalysisPattern(state) {
			this->element_kind = ElementKind::DeconstructorPattern;
		}

		~DeconstructorPattern() final = default;

		static MBox<DeconstructorPattern> parse(LangParserState& state);
		void                              dprint(std::ostream& out) const final;

		[[nodiscard]] 
		AccessLocked<IdentifierWrapper> getDeconstructorName() const { 
			return deconstructor_name.give(); 
		}

		[[nodiscard]] const AccessLocked<FlowPatternList> getArguments() const {
			return arguments.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Deconstructor Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
