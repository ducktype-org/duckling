#include "pst_test_utils.hpp"

#include <base/variant.hpp>

namespace pst {
	void viewAllSubTreeElementsAux(
		std::vector<AccessLocked<pst::LangElement>>& output, AccessLocked<pst::LangElement> root
	) {
		output.push_back(root);
		for (const auto& sub: root.illegalAccess().value()->viewChildren())
			viewAllSubTreeElementsAux(output, sub);
	}

	std::vector<AccessLocked<pst::LangElement>> viewAllSubTreeElements(
		AccessLocked<pst::LangElement> root
	) {
		std::vector<AccessLocked<pst::LangElement>> result;
		viewAllSubTreeElementsAux(result, root);
		return result;
	}
}
