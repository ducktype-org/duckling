// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "pst_parent.hpp"

#include <helios_private/templates/templates.hpp>

namespace compiler::helios {

	PSTParentResult getPSTElementParent(
		[[maybe_unused]] query::Context& ctx, pst::Access<pst::LangElement> element
	) {
		auto maybe_element_parent = element->getParent();
		if (maybe_element_parent.has_value()) {
			return PSTParentResult{ maybe_element_parent.value() };
		} else {
			// If there is no PST parent, we inspect the additional root data.

			const auto& additional_root_data = element->getAdditionalRootData();

			variant_match(additional_root_data.pst_parent) {
				variant_case(pst::AdditionalRootData::MacroExpansionParent, macro_parent) {
					return PSTParentResult{ macro_parent.expand_element };
				}
				variant_case(pst::AdditionalRootData::ModuleParent, module_parent) {
					const auto& module_id_any = module_parent.module_id;
					auto        module_id     = base::anyCast<frontend::ModuleID>(module_id_any);
					return PSTParentResult{ module_id };
				}
				variant_case(pst::AdditionalRootData::BakedTemplateParent, template_parent) {
					const auto& template_bake_data_any = template_parent.template_bake_data;
					auto        template_bake_data
						= base::anyCast<templates::TemplateBakePSTLinkedData>(template_bake_data_any
					    );
					return PSTParentResult{ template_bake_data.pst_parent_element };
				}
				variant_default {
					CORE_PANIC(
						"Element has no PST parent and unknown PST additional root data, cannot "
						"determine PST parent."
					);
				}
			}
			CORE_UNREACHABLE();
		}
	}
}
