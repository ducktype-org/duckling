// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "hierarchy/not_statements/synthetic_elements/template_expansion_assignment.hpp"
#include "hierarchy/not_statements/synthetic_elements/template_top_level.hpp"

namespace pst {
	class ElementSynthesizer final {
	public:
		static Box<TemplateExpansionAssignment> templateExpansionAssignment(
			LangParserElementConstructionData  data,
			MBox<IdentifierWrapper>&&          name,
			base::Optional<MBox<ExprHolder>>&& type,
			MBox<ExprHolder>&&                 value
		);
		static Box<TemplateTopLevel> templateTopLevel(
			LangParserElementConstructionData                data,
			std::vector<MBox<TemplateExpansionAssignment>>&& assignments,
			MBox<Stmt>&&                                     stmt
		);
	};
}
