// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../base_element/lang_element_construction_data.hpp"
#include "elements_list.hpp"

#include <base/pointers/box.hpp>

namespace pst {
	/**
	 * @brief Class with constructors for synthesized (Created outside of normal parsing) elements.
	 *
	 * Implementations are available in the implementations of these elements.
	 */
	class ElementSynthesizer final {
	public:
		static Box<TemplateExpansionAssignment> templateExpansionAssignment(
			LangElementConstructionData        data,
			MBox<IdentifierWrapper>&&          name,
			base::Optional<MBox<ExprHolder>>&& type,
			MBox<ExprHolder>&&                 value
		);
		static Box<TemplateTopLevel> templateTopLevel(
			LangElementConstructionData                      data,
			std::vector<MBox<TemplateExpansionAssignment>>&& assignments,
			MBox<Stmt>&&                                     stmt
		);
	};
}
