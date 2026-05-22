#include "pst_test_utils.hpp"

#include <set>

namespace pst {
	void viewAllSubTreeElementsAux(
		std::vector<AccessLocked<pst::LangElement>>& output, AccessLocked<pst::LangElement> root
	) {
		if (!root.illegalAccess().has_value()) return;
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
		std::set<std::string>                      paths;
		std::set<hashing::ComponentHash::HashType> hashes;

		auto elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			IF_BUILD_TYPE_DEV({
				std::string path = element.illegalAccess().value()->getElementPathHash().str();
				if (paths.contains(path)) {
					std::cerr << "BAD path: " << path << "\n";
					return base::BAD;
				}
				paths.insert(path);
			});

			auto hash = element.illegalAccess().value()->getElementPathHash().hash;
			if (hashes.contains(hash)) return base::BAD;
			hashes.insert(hash);
		}
		return base::OK;
	}

	base::OkBad checkUniqueHashes(AccessLocked<pst::LangElement> root) {
		std::set<HashType> hashes;
		auto               elements = viewAllSubTreeElements(root);
		for (auto& element: elements) {
			HashType hash = element.illegalAccess().value()->getHash();
			if (hashes.contains(hash)) return base::BAD;
			hashes.insert(hash);
		}
		return base::OK;
	}
}
