#pragma once

#include "../lir_unit_with_name.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/context/context_fd.hpp>

namespace compiler::driver {

	/**
	 * @brief Compiles the LIRUnitWithBackendName to LLVM Module.
	 * @TODO: #2246 Remove this function. Also: maybe remove LIRUnitWithBackendName
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
