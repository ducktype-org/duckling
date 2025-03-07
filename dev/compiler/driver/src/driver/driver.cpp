#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <system_command/system_command.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <backends/llvm/llvm_backend.hpp>
#include <backends/dvm/backend.hpp>

#include "driver.hpp"
#include "base/exceptions.hpp"

namespace compiler::driver {

	Box<BackendDriver> createBackendDriver(CRef<Options> options) {
		switch (options->backend_type) {
		case BackendType::LLVM:
			return base::makeBox<LLVMBackendDriver>(options);
		case BackendType::DuckBC:
			return base::makeBox<DuckBCBackendDriver>(options);
		default:
			CORE_PANIC("Wrong enum value");
		}
	}

	void LLVMBackendDriver::compile(const BackendModuleData& lir_module) {
		backend_llvm::Module mod(lir_module.module_id);
		for (const auto& lir_function: lir_module.functions) mod.addFunctionToModule(lir_function);

		if (not mod.verify()) CORE_PANIC("LLVM module verification failed");

		if (options->dump_llvm_ir) {
			base::StrID llvm_ir_path
				= base::StrID(base::strConcat(lir_module.module_id.strView(), ".ll").c_str());
			mod.debugDumpToFile(llvm_ir_path);
		}

		if (options->compile_to_assembly) {
			base::StrID assembly_path
				= base::StrID(base::strConcat(lir_module.module_id.strView(), ".s").c_str());
			mod.compile(assembly_path, backend_llvm::CompilationOutputType::Assembly);
			return;
		}

		// @TODO there should be one instance for all duck compiler options
		// and it should be passed to the backend drivers
		base::StrID object_file_path
			= base::StrID(base::strConcat(lir_module.module_id.strView(), ".o").c_str());
		mod.compile(object_file_path, backend_llvm::CompilationOutputType::Object);

		// Link the object file.
		// Use the default system linker - for Ubuntu it is advised to use gcc.
		// Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand(base::StrID("gcc"))
			.addArg(base::StrID("-o"))
			.addArg(options->output_file)
			.addArg(object_file_path)
			.execute();
	}

	void Driver::compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id) {
		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			auto mir_function = query::entryPoint<mir::LowerToMirFunction>({ hout_function });
			auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		BackendModuleData module_data{ .module_id = module_id, .functions = functions };

		backend_driver->compile(module_data);
	}

	void DuckBCBackendDriver::compile(const BackendModuleData& lir_module) {
		backend_vm::Module mod(lir_module.module_id);
		for (const auto& lir_function: lir_module.functions) mod.addLirFunction(lir_function);
		throw base::NotYetImplemented("Creating files for DuckBC backend");
	}
}
