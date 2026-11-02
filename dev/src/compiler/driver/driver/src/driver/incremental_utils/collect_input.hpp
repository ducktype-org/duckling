// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/parsed_pst.hpp>

#include <query_framework/external/api.hpp>

#include <vector>

namespace compiler::driver {
	/**
	 * Collect PST access side inputs for root and all subtree elements.
	 */
	void collectQueryInputsFromPst(
		CRef<pst::ParsedPST<>> pst_ref, std::vector<query::external::InputData>& out
	);

	/**
	 * Collect InputData "ids" (i.e. side input hashes) across all modules in global packages.
	 * This includes:
	 * - Module side inputs for all modules in the package.
	 * - File side inputs for all source files in the package.
	 * - PST access side inputs for all PST elements in the package.
	 * - Source file count and submodule count side inputs for all modules.
	 * - Module child side inputs for all module lookups performed in the previous compilation, that
	 * are still valid in the current module tree.
	 *
	 * This function uses previous metadata storage.
	 * @return A vector of InputData containing SideInput hashes and their corresponding QueryIDs.
	 */
	std::vector<query::external::InputData> collectInputDataFromGlobalPackagesFromPrevMetadata();

	/**
	 * Same as `collectInputDataFromGlobalPackagesFromPrevMetadata` but collects from
	 * the current metadata storage instead of the previous one.
	 * The main user is the Language Server.
	 */
	std::vector<query::external::InputData> collectInputDataFromGlobalPackagesFromCurrentMetadata();
}  // namespace compiler::driver
