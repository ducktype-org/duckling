// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lir_unit_with_name.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

#include "base/collections/optional.hpp"
#include "base/pointers/ref.hpp"

#include <fstream>
#include <optional>
#include <ostream>

namespace compiler::driver {

	/**
	 * @brief Utility function to get an ofstream for dumping debug artifacts.
	 * The artifact will be created if it does not exist.
	 */
	std::ofstream getDebugDumpArtifact(base::StrID artifact_name);

	/**
	 * @brief Converts HOUTUnit to LIRUnitWithBackendName.
	 * @note It also prints and/or saves in artifacts IR representations if requested by options.
	 * @param dump_lir Whether to write this unit's own LIR artifact when LIR dumping is enabled.
	 * @param aggregate_mir_dump Optional destination for requested MIR dumps instead of a per-unit
	 * file.
	 * @param aggregate_hir_dump Optional destination for requested HIR dumps instead of a per-unit
	 * file. Aggregated dumps are prefixed with the representation and module name.
	 */
	query::QResult<LIRUnitWithBackendName> compileHOUTUnitToLIRModuleData(
		query::Context&                         ctx,
		const helios::HOUTUnit&                 hout_unit,
		base::StrID                             module_id,
		base::StrID                             module_id_human,
		bool                                    dump_lir           = true,
		base::Optional<base::Ref<std::ostream>> aggregate_mir_dump = std::nullopt,
		base::Optional<base::Ref<std::ostream>> aggregate_hir_dump = std::nullopt
	);

	/**
	 * @brief Produces LIRUnitWithBackendName for given Duckling module.
	 */
	query::QResult<LIRUnitWithBackendName> compileModuleToLIRModuleData(
		query::Context& ctx, frontend::ModuleID module_id
	);
}
