// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../lir_unit_with_name.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <query_framework/context/context_fd.hpp>

namespace compiler::driver {

	/**
	 * @brief Compiles the LIRUnitWithBackendName to LLVM Module.
	 */
	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module
	);
}
