#include "compiler.hpp"

#include "context.hpp"
#include "function_pass.hpp"
#include "global_pass.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

namespace {
	/**
	 * @brief Turns modules into executables by emitting object files and invoking the system linker.
	 */
	void makeExecutable(pg::LlvmProgramModule& module, std::string_view output_path) {
		const auto executable_path = std::filesystem::path(std::string(output_path));
		auto       object_path     = executable_path;
		object_path += ".o";

		module.emitObjectFile(object_path);

		system_command::SystemCommand linker("clang");
		linker.addArg(object_path.string());
		linker.addArg("-o");
		linker.addArg(executable_path.string());
		if (linker.execute(system_command::SystemCommand::ExitCodeHandling::Warn) != 0)
			CORE_PANIC("Failed to link LLVM object into executable");

		CORE_DEV_LOG(Compiler, "Wrote executable to ", executable_path.string(), '\n');
	}
}

void pg::compile(const fs::File& file, std::string_view output_path) {
	CORE_DEV_LOG(
		Compiler,
		"Starting compilation process of ",
		file.getFilePath().string(),
		" to ",
		std::string(output_path),
		'\n'
	);

	pst::PST pst(file, pst::PSTType::Program);

	LlvmProgramModule   llvm_module;
	LlvmLoweringContext llvm_lowering(llvm_module);
	Context             ctx(llvm_lowering);

	// Pass 1: register top-level declarations (namespaces, functions, structs) in Context.
	GlobalPass global_pass(ctx);
	for (pst::AccessLocked<pst::Stmt> inner_stmt:
	     pst.getRootElement().illegalAccess().value()->getStatements()) {
		inner_stmt.illegalAccess().value()->acceptVisitor(global_pass);
	}

	// Pass 2: lower function bodies into LLVM IR.
	FunctionPass function_pass(ctx, llvm_module, llvm_lowering);
	for (pst::AccessLocked<pst::Stmt> inner_stmt:
	     pst.getRootElement().illegalAccess().value()->getStatements()) {
		inner_stmt.illegalAccess().value()->acceptVisitor(function_pass);
	}

	llvm_module.verify();
	makeExecutable(llvm_module, output_path);
}
