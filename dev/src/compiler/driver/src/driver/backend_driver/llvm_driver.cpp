#include "llvm_driver.hpp"

#include "llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <system_command/system_command.hpp>

namespace compiler::driver {

	void LLVMDriver::compileModule(query::Context& ctx, const BackendModuleData& lir_module, artifacts::FileArtifact output_artifact) {
		backend_llvm::Module mod(lir_module.module_id);
		for (const auto& lir_function: lir_module.functions)
			mod.addFunctionToModule(ctx, lir_function);

		if (mod.verify().isBad()) CORE_PANIC("LLVM module verification failed");

		if (options->dump_llvm_ir) {
			base::StrID llvm_ir_path
				= base::StrID(base::strConcat(lir_module.module_id.strView(), ".ll").c_str());
			mod.debugDumpToFile(llvm_ir_path);
		}

		if (options->compile_to_assembly) {
			base::StrID assembly_path
				= base::StrID(base::strConcat(lir_module.module_id.strView(), ".s").c_str());
			mod.compile(assembly_path.strView(), backend_llvm::CompilationOutputType::Assembly);
		}

		// @TODO there should be one instance for all duck compiler options
		// and it should be passed to the backend drivers

		mod.compile(output_artifact.FILE, backend_llvm::CompilationOutputType::Object);
		object_file_paths.push_back(output_artifact.FILE);
	}

}
