/**
 * @file compile_llvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_llvm.hpp"

#include "builtins_registry.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <global_state/artifacts_location.hpp>
#include <lir/lir_structure/lir_structure_fd.hpp>
#include <time_stats/time_stats.hpp>

namespace compiler::driver {

	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);
		return backend_llvm::Module::fromLIRUnit(ctx, lir_module->lir_unit, lir_module->module_id);
	}

	artifacts::FileArtifact emitBuiltinLLVMObjectFile() {
		auto builtin_obj_file
			= global_state::getRootCollection()->fileArtifactAtOrNew(base::StrID("builtins_llvm.o"));
		auto mod = backend_llvm::Module::fromLLVMBC(getBuiltinsX8664LinuxGnuBCSpan());
		mod.compile(
			builtin_obj_file.file.getFilePath(), backend_llvm::CompilationOutputType::Object
		);
		return builtin_obj_file;
	}
}
