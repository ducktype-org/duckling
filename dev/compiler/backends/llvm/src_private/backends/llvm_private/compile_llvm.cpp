#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
LLVM_INCLUDE_END()

#include "compile_llvm.hpp"
#include "module_impl.hpp"

#include <base/box.hpp>
#include <base/exceptions.hpp>

namespace compiler::backend_llvm {

	Ref<llvm::TargetMachine> getTargetMachine(const std::string& target_triple) {
		if (target_triple == llvm::sys::getDefaultTargetTriple()) {
			if (llvm::InitializeNativeTarget())
				CORE_PANIC("LLVM error: failed to initialize native target");
			if (llvm::InitializeNativeTargetAsmPrinter())
				CORE_PANIC("LLVM error: failed to initialize native target asm printer");
		} else {
			throw base::NotYetImplemented("target different than native");
		}

		std::string error;
		auto        target = llvm::TargetRegistry::lookupTarget(target_triple, error);

		// Error if we couldn't find the requested target.
		if (!target) CORE_PANIC("LLVM error: " + error);

		auto cpu      = "generic";
		auto features = "";

		llvm::TargetOptions opt;
		return target->createTargetMachine(target_triple, cpu, features, opt, llvm::Reloc::PIC_);
	}

	void emitCode(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		Ref<llvm::raw_pwrite_stream> output_stream,
		llvm::CodeGenFileType        file_type
	) {
		// It's the only way to emit a file with a target machine (despite the "legacy" name)
		llvm::legacy::PassManager pass;
		m->setDataLayout(target_machine->createDataLayout());
		m->setTargetTriple(target_machine->getTargetTriple().getTriple());

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
		Ref<ModuleImpl> module_impl, base::StrID output_file, CompilationOutputType output_type
	) {
		auto m              = module_impl->module.refMut();
		auto target_triple  = llvm::sys::getDefaultTargetTriple();
		auto target_machine = getTargetMachine(target_triple);

		std::error_code error_code;

		// Using raw_fd_ostream is the recommended way to write files in LLVM,
		// as it is much more efficient than using std::ofstream.
		llvm::raw_fd_ostream output_stream(
			output_file.strView(), error_code, llvm::sys::fs::OF_None
		);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		if (output_type == CompilationOutputType::Assembly)
			emitAssembly(m, target_machine, &output_stream);
		else
			emitObject(m, target_machine, &output_stream);

		llvm::errs() << "Compiled LLVM module to the file: " << output_file.strView() << "\n";
	}
}
