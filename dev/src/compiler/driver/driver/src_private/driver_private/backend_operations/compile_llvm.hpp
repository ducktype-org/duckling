#pragma once

#include "../lir_module_data.hpp"

#include <backends/llvm/llvm_backend.hpp>

#include <artifacts/artifacts.hpp>
#include <query_frameworkcontext/context_fd.hpp>

namespace compiler::driver {

	/**
	 * @brief Compiles the LIRModuleData to LLVM Module.
	 */
	backend_llvm::Module compileLIRModuleToLLVM(query::Context& ctx, const LIRModuleData& lir_module);


	/**
	 * Compile builtin LLVM library into an object file.
	 */
	artifacts::FileArtifact emitBuiltinLLVMObjectFile();
}
