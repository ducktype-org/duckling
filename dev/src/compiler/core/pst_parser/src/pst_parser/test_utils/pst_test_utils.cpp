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

	base::OkBad checkUniqueElementPaths(AccessLocked<pst::LangElement> root) {
		std::set<std::string> paths;
		auto                  elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			std::string path = element.illegalAccess().value()->getElementPath().str();
			if (paths.contains(path)) return base::BAD;
			paths.insert(path);
		}
		return base::OK;
	}

	base::OkBad checkUniqueHashes(AccessLocked<pst::LangElement> root) {
		std::set<u64> hashes;
		auto          elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			u64 hash = element.illegalAccess().value()->getHash();
			if (hashes.contains(hash)) return base::BAD;
			hashes.insert(hash);
		}
		return base::OK;
	}
}
