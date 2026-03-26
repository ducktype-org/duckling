#pragma once
#include "../../lists/flow_pattern_list.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a deconstructor pattern.
	 */
	class DeconstructorPattern final: public AnalysisPattern {
		tpc::Identifier deconstructor_name;
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

		[[nodiscard]] base::StrID getDeconstructorName() const { return deconstructor_name.value; }

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
