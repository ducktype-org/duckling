#include "backends/llvm/detail/module_impl.hpp"
#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>

#include <llvm/Target/TargetMachine.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Support/CodeGen.h>
LLVM_INCLUDE_END()

#include <base/exceptions.hpp>
#include <base/box.hpp>

#include "compile_llvm.hpp"

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
		const std::string&           target_triple,
		Ref<llvm::raw_pwrite_stream> output_stream,
		llvm::CodeGenFileType        file_type
	) {
		llvm::legacy::PassManager pass;
		m->setDataLayout(target_machine->createDataLayout());
		m->setTargetTriple(target_triple);

		if (target_machine->addPassesToEmitFile(pass, *output_stream, nullptr, file_type))
			CORE_PANIC("TargetMachine can't emit a file of this type");

		pass.run(*m);
		output_stream->flush();
	}

	void emitObject(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		const std::string&           target_triple,
		Ref<llvm::raw_pwrite_stream> output_stream
	) {
		emitCode(
			m, target_machine, target_triple, output_stream, llvm::CodeGenFileType::ObjectFile
		);
	}

	void emitAssembly(
		Ref<llvm::Module>            m,
		Ref<llvm::TargetMachine>     target_machine,
		const std::string&           target_triple,
		Ref<llvm::raw_pwrite_stream> output_stream
	) {
		emitCode(
			m, target_machine, target_triple, output_stream, llvm::CodeGenFileType::AssemblyFile
		);
	}

	/**
	 * @brief
	 *
	 * One can also inspect the contents of the binary object file with tools like `objdump` and
	 * `readelf`.
	 */
	void
		compileModuleToObject(Ref<ModuleImpl> module_impl, const ModuleCompilationOptions& options) {
		auto m              = module_impl->module.refMut();
		auto target_triple  = llvm::sys::getDefaultTargetTriple();
		auto target_machine = getTargetMachine(target_triple);

		std::error_code      error_code;

		// Using raw_fd_ostream is the recommended way to write files in LLVM, 
		// as it is much more efficient than using std::ofstream.
		llvm::raw_fd_ostream output_stream(
			options.object_file_path.strView(), error_code, llvm::sys::fs::OF_None
		);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		if (options.output_type == ModuleCompilationOptions::OutputType::Assembly)
			emitAssembly(m, target_machine, target_triple, &output_stream);
		else
			emitObject(m, target_machine, target_triple, &output_stream);

		llvm::errs() << "Generated object file: " << options.object_file_path.strView() << "\n";

		if (options.llvm_ir_path.has_value()) {
			llvm::raw_fd_ostream ir_output_stream(
				options.llvm_ir_path->str(), error_code, llvm::sys::fs::OF_None
			);
			if (error_code)
				CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

			m->print(ir_output_stream, nullptr);
		}
	}
}
