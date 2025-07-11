#pragma once

#include "../backend_module_data.hpp"

#include <artifacts/artifacts.hpp>
#include <backends/llvm/llvm_backend.hpp>
#include <query_framework/context_fd.hpp>

namespace compiler::driver {

	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, const LIRModuleData& lir_module
		// const artifacts::FileArtifact& output_artifact
	);


	/**
	 * Compile builtin LLVM library into an object file.
	 */
	artifacts::FileArtifact emitBuiltinLLVMObjectFile();
}
