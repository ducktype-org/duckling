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
