// SPDX-License-Identifier: MIT
#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <unordered_set>

namespace compiler::driver {

	// Collect hashes of all PST elements across all modules in global packages.
	// Returns an unordered_set of element hashes.
	std::unordered_set<pst::LangElement::HashType> collectAllPstElementHashesFromGlobalPackages();


}  // namespace compiler::driver
