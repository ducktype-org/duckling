#include "llvm_driver.hpp"

#include "llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <system_command/system_command.hpp>

namespace compiler::driver {

	void LLVMDriver::compileModule(query::Context& ctx, const BackendModuleData& lir_module) {
		backend_llvm::Module mod(lir_module.module_id);

		std::vector<CRef<lir::Function>> ctors;
		std::vector<CRef<lir::Function>> dtors;

		for (const auto& global: lir_module.globals) {
			const auto& lir_global = std::get<0>(global);
			const auto& ctor_opt   = std::get<1>(global);
			const auto& dtor_opt   = std::get<2>(global);
			mod.addGlobalToModule(lir_global);
			if (ctor_opt.has_value()) {
				mod.addFunctionToModule(ctx, ctor_opt.value());
				ctors.push_back(ctor_opt.value());
			}
			if (dtor_opt.has_value()) {
				mod.addFunctionToModule(ctx, dtor_opt.value());
				dtors.push_back(dtor_opt.value());
			}
		}

		if (!ctors.empty()) {
			auto module_ctor = lir::fromLIRFunctions(
				ctx,
				ctors,
				base::StrID(base::strConcat("_CTOR_MODULE_", lir_module.module_id.str()).c_str())
			);
			mod.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));
		}

		if (!dtors.empty()) {
			std::vector<CRef<lir::Function>> reversed_dtors(dtors.rbegin(), dtors.rend());
			auto                             module_dtor = lir::fromLIRFunctions(
                ctx,
                reversed_dtors,
                base::StrID(base::strConcat("_DTOR_MODULE_", lir_module.module_id.str()).c_str())
            );
			mod.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
		}

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
			mod.compile(assembly_path, backend_llvm::CompilationOutputType::Assembly);
		}

		// @TODO there should be one instance for all duck compiler options
		// and it should be passed to the backend drivers
		base::StrID object_file_path
			= base::StrID(base::strConcat(lir_module.module_id.strView(), ".o").c_str());

		mod.compile(object_file_path, backend_llvm::CompilationOutputType::Object);
		object_file_paths.push_back(object_file_path);
	}

	void LLVMDriver::link(base::StrID output_file) {
		if (options->add_builtin_library) {
			auto mod                 = backend_llvm::Module::fromIRCode(LLVM_IR_LIB);
			auto builtin_object_path = base::StrID("builtin.o");
			mod.compile(builtin_object_path, backend_llvm::CompilationOutputType::Object);
			object_file_paths.push_back(builtin_object_path);
		}

		// Link the object file.
		// Use the default system linker - for Ubuntu it is advised to use gcc.
		// Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand command("gcc");

		for (const auto& object_file_path: object_file_paths)
			command.addArg(object_file_path.str());

		for (const auto& external_object_file: options->external_objects_files)
			command.addArg(external_object_file.str());

		for (const auto& external_lib: options->external_libs)
			command.addArg(base::strConcat("-l", external_lib.strView()));

		command.addArg("-lc");  // Link the C standard library.

		command.addArg("-o");
		command.addArg(output_file.str());
		command.execute();
	}
}
