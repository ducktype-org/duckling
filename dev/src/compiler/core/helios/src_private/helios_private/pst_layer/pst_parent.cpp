#include "pst_parent.hpp"

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
					auto module_id_any = module_parent.module_id;
					auto module_id     = base::anyCast<frontend::ModuleID>(module_id_any);
					return PSTParentResult{ module_id };
				}
				variant_default {
					CORE_PANIC(
						"Element has no parent and no additional root data, cannot "
						"determine scope parent."
					);
				}
			}
			CORE_UNREACHABLE();
		}
	}
}
