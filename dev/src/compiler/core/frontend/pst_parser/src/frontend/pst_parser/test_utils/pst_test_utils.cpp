// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "pst_test_utils.hpp"

#include <set>
#include <sstream>

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

	void printSubElementTypes(CRef<LangElement> el) {
		std::cerr << el->elementType() << ", ";
		for (auto& sub: el->viewChildren())
			printSubElementTypes(base::CRef(&*sub.illegalAccess().value()));
	}

	u64 countSubElements(CRef<LangElement> el) {
		u64 count = 1;
		for (auto& sub: el->viewChildren())
			count += countSubElements(base::CRef(&*sub.illegalAccess().value()));
		return count;
	}

	base::OkBad testElementCloning(base::CRef<LangElement> el) {
		u64 x1 = 0, x2 = 0, x3 = 0, x4 = 0;
		{
			x1         = countSubElements(el);
			auto clone = el->clone();

			if (!clone) {
				std::println(std::cerr, "[Error] Cloning failed");
				return base::BAD;
			}

			x2 = countSubElements(el);
			x3 = countSubElements(clone.ref().toOpt().value());

			std::stringstream el_dprint_ss, clone_dprint_ss;

			tpc::nullAwareDprint(MRef{ el }, el_dprint_ss);
			tpc::nullAwareDprint(clone, clone_dprint_ss);

			std::string el_dprint = el_dprint_ss.str(), clone_dprint = clone_dprint_ss.str();

			// This might break if positions are no longer the same
			if (!(x1 == x3) || el_dprint != clone_dprint) {
				std::println(std::cerr, "[Error] Cloning has bad output");

				std::cerr << el_dprint << "\n" << clone_dprint << "\n";
				printSubElementTypes(el);
				std::cerr << "\n";
				printSubElementTypes(clone.ref().toOpt().value());
				std::cerr << "\n";

				return base::BAD;
			}
		}
		x4 = countSubElements(el);
		if (x1 == x2 && x2 == x3 && x3 == x4)
			return base::OK;
		else
			return base::BAD;
	}
}
