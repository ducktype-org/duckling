#include "pst_test_utils.hpp"
#include <base/variant.hpp>

namespace pst {
	void viewAllSubTreeElementsAux(
		std::vector<CRef<pst::LangElement>>& output, CRef<pst::LangElement> root
	) {
		output.push_back(root);
		for (const auto& sub: root->viewSubElements()) {
			variant_match(sub) {
				variant_case(pst::LangElement::ConstChild, sub) {
					viewAllSubTreeElementsAux(output, sub);
				}
				variant_default {}
			}
		}
	}

	std::vector<CRef<pst::LangElement>> viewAllSubTreeElements(CRef<pst::LangElement> root) {
		std::vector<CRef<pst::LangElement>> result;
		viewAllSubTreeElementsAux(result, root);
		return result;
	}
}
