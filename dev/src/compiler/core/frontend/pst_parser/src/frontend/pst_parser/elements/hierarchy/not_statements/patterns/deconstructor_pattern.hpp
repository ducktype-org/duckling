// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "../../lists/flow_pattern_list.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a deconstructor pattern.
	 */
	class DeconstructorPattern final: public AnalysisPattern {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(DeconstructorPattern, AnalysisPattern);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(deconstructor_name, IdentifierWrapper);
		NAMED_CHILD(arguments, FlowPatternList);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit DeconstructorPattern(LangElementConstructionArgument state):
			  AnalysisPattern(state) {
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
