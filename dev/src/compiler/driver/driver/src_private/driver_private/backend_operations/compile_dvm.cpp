// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file compile_dvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_dvm.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <time_stats/time_stats.hpp>

#include <logger/logger.hpp>

namespace compiler::driver {


	DVMModuleData compileLIRModuleToDVM(
		CRef<LIRUnitWithBackendName> data, query::Context& query_ctx, bool build_debug_info
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

		backend_vm::DVMCodeBuilder module(query_ctx, data->module_id, build_debug_info, false);

		module.insertLIRUnit(data->lir_unit);

		auto code_collection = module.build();

		base::Optional<debug_info::DebugInfo> debug_info_opt;
		if (build_debug_info) debug_info_opt = module.buildDebugInfo();

		return {
			.code       = std::move(code_collection),
			.debug_info = std::move(debug_info_opt),
		};
	}
}
