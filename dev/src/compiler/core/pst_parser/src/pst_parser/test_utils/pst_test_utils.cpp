#include "pst_test_utils.hpp"

#include <base/variant.hpp>

#include <set>

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

	bool checkUniqueElementPaths(AccessLocked<pst::LangElement> root) {
		std::set<std::string> paths;
		auto                  elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			std::string path = element.illegalAccess().value()->getElementPath().str();
			if (paths.contains(path)) return false;
			paths.insert(path);
		}
		return true;
	}
}
