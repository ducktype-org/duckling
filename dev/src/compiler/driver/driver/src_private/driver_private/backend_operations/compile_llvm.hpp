#pragma once

#include "../lir_unit_with_name.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/context/context_fd.hpp>

namespace compiler::driver {

	/**
	 * @brief Compiles the LIRUnitWithBackendName to LLVM Module.
	 * @TODO: #2246 Move this logic to the LLVM backend, it is not really driver-specific and it depends on LIR structure anyway.
	 * Also: maybe remove LIRUnitWithBackendName -- we can just set module name per module in backend.
	 */
	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module
	);


	/**
	 * Compile builtin LLVM library into an object file.
	 * @TODO: #2246 this might be moved to backend.
	 */
	artifacts::FileArtifact emitBuiltinLLVMObjectFile();
}
