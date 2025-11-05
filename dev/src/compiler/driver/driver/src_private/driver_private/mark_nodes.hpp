// SPDX-License-Identifier: MIT
#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <query_framework/external/api.hpp>

#include <vector>

namespace compiler::driver {

	/**
	 * Collect hashes of all PST elements across all modules in global packages.
	 * @return A vector of InputData containing PST element hashes and their corresponding QueryIDs.
	 */
	std::vector<query::external::InputData> collectAllPstElementHashesFromGlobalPackages();

}  // namespace compiler::driver
