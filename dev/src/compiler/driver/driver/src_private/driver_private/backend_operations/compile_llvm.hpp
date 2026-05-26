#pragma once

#include "../lir_module_data.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/context/context_fd.hpp>

namespace compiler::driver {

	/**
	 * @brief Compiles the LIRUnitWithBackendName to LLVM Module.
	 #2246 this should be moved to backend probably, or some pipeline module/submodule
	 */
	backend_llvm::Module compileLIRModuleToLLVM(query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module);


	/**
	 * Compile builtin LLVM library into an object file.
	 #2246 this should be moved to backend probably, or some pipeline module/submodule
	 */
	artifacts::FileArtifact emitBuiltinLLVMObjectFile();
}
