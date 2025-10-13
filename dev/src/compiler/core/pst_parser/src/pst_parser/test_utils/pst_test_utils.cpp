#include "pst_test_utils.hpp"

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

	base::OkBad checkUniqueComponentHashs(AccessLocked<pst::LangElement> root) {
		std::set<std::string> paths;
		auto                  elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			std::string path = element.illegalAccess().value()->getComponentHash().str();
			if (paths.contains(path)) return base::BAD;
			paths.insert(path);
		}
		return base::OK;
	}

	base::OkBad checkUniqueHashes(AccessLocked<pst::LangElement> root) {
		std::set<LangElement::HashType> hashes;
		auto                            elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			LangElement::HashType hash = element.illegalAccess().value()->getHash();
			if (hashes.contains(hash)) return base::BAD;
			hashes.insert(hash);
		}
		return base::OK;
	}
}
