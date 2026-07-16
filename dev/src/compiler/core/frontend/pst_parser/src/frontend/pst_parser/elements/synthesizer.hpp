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
