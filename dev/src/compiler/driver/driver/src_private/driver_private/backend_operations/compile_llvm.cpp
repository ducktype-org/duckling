// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file compile_llvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_llvm.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <lir/lir_structure/lir_structure_fd.hpp>
#include <time_stats/time_stats.hpp>

namespace compiler::driver {

	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);
		return backend_llvm::Module::fromLIRUnit(ctx, lir_module->lir_unit, lir_module->module_id);
	}
}
