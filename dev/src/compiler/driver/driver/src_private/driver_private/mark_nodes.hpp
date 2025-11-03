// SPDX-License-Identifier: MIT
#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <unordered_set>

namespace compiler::driver {

	/**
	 * Collect hashes of all PST elements across all modules in global packages.
	 * Returns an unordered_set of element hashes.
	 */
	std::unordered_set<pst::LangElement::HashType> collectAllPstElementHashesFromGlobalPackages();

	/**
	 * Mark Inputs and Side Inputs in the previous query graph as Green if their hash is present
	 * in the set of PST element hashes collected from global packages, otherwise mark them as Red.
	 * Other nodes are not affected.
	 */
	void markPreviousGraphNodesInputs();

}  // namespace compiler::driver
