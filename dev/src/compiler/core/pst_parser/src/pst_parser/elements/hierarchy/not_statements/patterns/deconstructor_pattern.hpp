#pragma once
#include "analysis_pattern.hpp"
#include "pst_parser/access.hpp"
#include "pst_parser/elements/hierarchy/lists/flow_pattern_list.hpp"

namespace pst {
	/**
	 * @brief Represents a deconstructor pattern.
	 */
	class DeconstructorPattern final: public AnalysisPattern {
		tpc::Identifier                 deconstructor_name;
		NAMED_CHILD(arguments, FlowPatternList);

	public:
		explicit DeconstructorPattern(const dia::SourcePosition& position):
			  AnalysisPattern(position) {
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
