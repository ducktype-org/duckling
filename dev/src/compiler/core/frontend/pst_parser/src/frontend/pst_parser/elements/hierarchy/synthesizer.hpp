#pragma once

#include "not_statements/synthetic_elements/template_expansion_assignments.hpp"
#include "not_statements/synthetic_elements/template_top_level.hpp"

namespace pst {
	class ElementSynthesizer final {
	public:
		static Box<TemplateExpansionAssignment> templateExpansionAssignment(
			Box<IdentifierWrapper>&&     name,
			base::Optional<ExprHolder>&& type,
			base::Optional<ExprHolder>&& value
		);
		static Box<TemplateExpansionAssignment> templateTopLevel(
			std::vector<Box<TemplateExpansionAssignment>>&& assignments, Box<Stmt>&& stmt
		);
	};
}
