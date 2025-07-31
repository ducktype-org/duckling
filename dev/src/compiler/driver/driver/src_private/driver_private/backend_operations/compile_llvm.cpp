#include "compile_llvm.hpp"

#include "llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <global_state/artifacts_location.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <system_command/system_command.hpp>

namespace compiler::driver {

	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, const LIRModuleData& lir_module
	) {
		backend_llvm::Module mod(lir_module.module_id);

		std::vector<CRef<lir::Function>> ctors;
		std::vector<CRef<lir::Function>> dtors;


		for (const auto& global: lir_module.globals) {
			mod.addGlobalToModule(global.lir_global);
			// Add global constructors and destructors if they exist
			if (global.global_ctor.has_value()) {
				mod.addFunctionToModule(ctx, global.global_ctor.value());
				ctors.push_back(global.global_ctor.value());
			}
			if (global.global_dtor.has_value()) {
				mod.addFunctionToModule(ctx, global.global_dtor.value());
				dtors.push_back(global.global_dtor.value());
			}
		}

		if (not ctors.empty()) {
			auto module_ctor = lir::fromLIRFunctions(
				ctx,
				ctors,
				// @TODO: Add suport to mangling ctors of globals to helios mangler #906
				base::StrID(base::strConcat("_CTOR_MODULE_", lir_module.module_id.str()).c_str())
			);
			mod.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));
		}

		if (not dtors.empty()) {
			// Dtors should be called in reverse order
			std::vector<CRef<lir::Function>> reversed_dtors(dtors.rbegin(), dtors.rend());
			auto                             module_dtor = lir::fromLIRFunctions(
                ctx,
                reversed_dtors,
                // @TODO: Add suport to mangling dtors of globals to helios mangler #906
                base::StrID(base::strConcat("_DTOR_MODULE_", lir_module.module_id.str()).c_str())
            );
			mod.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
		}

		for (const auto& lir_function: lir_module.functions)
			mod.addFunctionToModule(ctx, lir_function);

		CORE_ASSERT(mod.verify().isOk(), "LLVM module verification failed");

		return mod;
	}

	artifacts::FileArtifact emitBuiltinLLVMObjectFile() {
		auto builtin_obj_file
			= global_state::getRootCollection()->fileArtifactAtOrNew(base::StrID("builtins_llvm.o"));
		auto mod = backend_llvm::Module::fromIRCode(LLVM_IR_LIB);
		mod.compile(
			builtin_obj_file.FILE.getFilePath(), backend_llvm::CompilationOutputType::Object
		);
		return builtin_obj_file;
	}
}
