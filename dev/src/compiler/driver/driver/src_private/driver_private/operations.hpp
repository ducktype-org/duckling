// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lir_unit_with_name.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

#include <fstream>

namespace compiler::driver {

	/**
	 * @brief Utility function to get an ofstream for dumping debug artifacts.
	 * The artifact will be created if it does not exist.
	 */
	std::ofstream getDebugDumpArtifact(base::StrID artifact_name);

	/**
	 * @brief Converts HOUTUnit to LIRUnitWithBackendName.
	 * @note It also prints and/or saves in artifacts IR representations if requested by options.
	 */
	query::QResult<LIRUnitWithBackendName> compileHOUTUnitToLIRModuleData(
		query::Context&         ctx,
		const helios::HOUTUnit& hout_unit,
		base::StrID             module_id,
		base::StrID             module_id_human,
		bool                    dump_lir           = true,
		std::ostream*           aggregate_mir_dump = nullptr,
		std::ostream*           aggregate_hir_dump = nullptr
	);

	/**
	 * @brief Produces LIRUnitWithBackendName for given Duckling module.
	 */
	query::QResult<LIRUnitWithBackendName> compileModuleToLIRModuleData(
		query::Context& ctx, frontend::ModuleID module_id
	);
}
