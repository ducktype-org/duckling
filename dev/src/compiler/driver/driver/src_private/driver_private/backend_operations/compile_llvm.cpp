#include "compile_llvm.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <system_command/system_command.hpp>

namespace compiler::driver {

	void compileBackendModuleToLLVM(
		query::Context&          ctx,
		const BackendModuleData& lir_module,
		const artifacts::FileArtifact&  output_artifact
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

		if (mod.verify().isBad()) CORE_PANIC("LLVM module verification failed");

		// TODO PR:
		// if (options->dump_llvm_ir) {
		// 	base::StrID llvm_ir_path
		// 		= base::StrID(base::strConcat(lir_module.module_id.strView(), ".ll").c_str());
		// 	mod.debugDumpToFile(llvm_ir_path);
		// }

		// if (options->compile_to_assembly) {
		// 	base::StrID assembly_path
		// 		= base::StrID(base::strConcat(lir_module.module_id.strView(), ".s").c_str());
		// 	mod.compile(assembly_path.strView(), backend_llvm::CompilationOutputType::Assembly);
		// }

		// @TODO there should be one instance for all duck compiler options
		// and it should be passed to the backend drivers

		mod.compile(output_artifact.FILE, backend_llvm::CompilationOutputType::Object);
	}
}
