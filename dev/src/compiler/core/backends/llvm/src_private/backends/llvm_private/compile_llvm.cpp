#include <global_state/backend_options.hpp>
#include <llvm_helpers/llvm_helpers.hpp>

#include <iostream>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
LLVM_INCLUDE_END()

#include "compile_llvm.hpp"
#include "module_impl.hpp"

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_llvm {
	using LLVMOptimizationLevel = global_state::BackendOptions::LLVMBackend::LLVMOptimizationLevel;

	/**
	 * @brief Convert our OptimizationLevel enum to LLVM's OptimizationLevel for IR generation.
	 */
	llvm::OptimizationLevel toLLVMOptLevel(const LLVMOptimizationLevel level) {
		switch (level) {
		case LLVMOptimizationLevel::O0:
			return llvm::OptimizationLevel::O0;
		case LLVMOptimizationLevel::O1:
			return llvm::OptimizationLevel::O1;
		case LLVMOptimizationLevel::O2:
			return llvm::OptimizationLevel::O2;
		case LLVMOptimizationLevel::O3:
			return llvm::OptimizationLevel::O3;
		case LLVMOptimizationLevel::Os:
			return llvm::OptimizationLevel::Os;
		case LLVMOptimizationLevel::Oz:
			return llvm::OptimizationLevel::Oz;
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Run LLVM IR optimization passes on the module.
	 */
	void runOptimizationPasses(
		const Ref<llvm::Module>        m,
		const Ref<llvm::TargetMachine> target_machine,
		const LLVMOptimizationLevel    level
	) {
		if (level == LLVMOptimizationLevel::O0) return;  // Skip optimization for O0

		llvm::LoopAnalysisManager     loop_analysis_manager;
		llvm::FunctionAnalysisManager function_analysis_manager;
		llvm::CGSCCAnalysisManager    cgscc_analysis_manager;
		llvm::ModuleAnalysisManager   module_analysis_manager;

		llvm::PassBuilder pass_builder(target_machine.get());

		pass_builder.registerModuleAnalyses(module_analysis_manager);
		pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
		pass_builder.registerFunctionAnalyses(function_analysis_manager);
		pass_builder.registerLoopAnalyses(loop_analysis_manager);
		pass_builder.crossRegisterProxies(
			loop_analysis_manager,
			function_analysis_manager,
			cgscc_analysis_manager,
			module_analysis_manager
		);

		llvm::ModulePassManager module_pass_manager
			= pass_builder.buildPerModuleDefaultPipeline(toLLVMOptLevel(level));
		module_pass_manager.run(*m, module_analysis_manager);
	}

	void emitCode(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		Ref<llvm::raw_pwrite_stream> output_stream,
		llvm::CodeGenFileType        file_type
	) {
		// It's the only way to emit a file with a target machine (despite the "legacy" name)
		llvm::legacy::PassManager pass;

		if (target_machine->addPassesToEmitFile(pass, *output_stream, nullptr, file_type))
			CORE_PANIC("TargetMachine can't emit a file of this type");

		pass.run(*m);
		output_stream->flush();
	}

	void emitObject(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		Ref<llvm::raw_pwrite_stream> output_stream
	) {
		emitCode(m, target_machine, output_stream, llvm::CodeGenFileType::ObjectFile);
	}

	void emitAssembly(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		Ref<llvm::raw_pwrite_stream> output_stream
	) {
		emitCode(m, target_machine, output_stream, llvm::CodeGenFileType::AssemblyFile);
	}

	/**
	 * @brief Compiles the module to an object file or assembly file.
	 */
	void compileModuleToObject(
		const Ref<ModuleImpl>        module_impl,
		const std::filesystem::path& output_file,
		const CompilationOutputType  output_type
	) {
		const auto m              = module_impl->module.refMut();
		const auto target_machine = module_impl->getTargetMachine().toOpt().value();

		// Run optimization passes before code generation
		runOptimizationPasses(
			m,
			target_machine,
			global_state::getBackendOptions()->llvm_backend->llvm_optimization_level
		);

		std::error_code error_code;

		// Using raw_fd_ostream is the recommended way to write files in LLVM,
		// as it is much more efficient than using std::ofstream.
		llvm::raw_fd_ostream output_stream(output_file.native(), error_code, llvm::sys::fs::OF_None);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		if (output_type == CompilationOutputType::Assembly)
			emitAssembly(m, target_machine, &output_stream);
		else
			emitObject(m, target_machine, &output_stream);

		CORE_DEV_LOG(Backend, "Compiled LLVM module to the file: ", output_file.string(), "\n");
	}
}
