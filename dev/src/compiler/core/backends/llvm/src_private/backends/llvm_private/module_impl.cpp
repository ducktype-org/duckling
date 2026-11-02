// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <global_state/backend_options.hpp>
#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
LLVM_INCLUDE_END()

#include "module_impl.hpp"

namespace compiler::backend_llvm {
	using LLVMOptimizationLevel = global_state::BackendOptions::LLVMBackend::LLVMOptimizationLevel;

	ModuleImpl::ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {
		const std::string target_triple = llvm::sys::getDefaultTargetTriple();
		setTargetMachine(target_triple);
		this->module->setDataLayout(target_machine->createDataLayout());
		this->module->setTargetTriple(target_machine->getTargetTriple().getTriple());
	}

	/**
	 * @brief Convert our OptimizationLevel enum to LLVM's CodeGenOptLevel for machine code gen.
	 */
	llvm::CodeGenOptLevel toLLVMCodeGenOptLevel(const LLVMOptimizationLevel level) {
		switch (level) {
		case LLVMOptimizationLevel::O0:
			return llvm::CodeGenOptLevel::None;
		case LLVMOptimizationLevel::O1:
			return llvm::CodeGenOptLevel::Less;
		case LLVMOptimizationLevel::O2:
			return llvm::CodeGenOptLevel::Default;
		case LLVMOptimizationLevel::O3:
			return llvm::CodeGenOptLevel::Aggressive;
		case LLVMOptimizationLevel::Os:
			return llvm::CodeGenOptLevel::Default;
		case LLVMOptimizationLevel::Oz:
			return llvm::CodeGenOptLevel::Default;
		}
		CORE_UNREACHABLE();
	}

	Ref<llvm::TargetMachine> ModuleImpl::setTargetMachine(const std::string& target_triple) {
		if (target_triple != llvm::sys::getDefaultTargetTriple())
			throw base::NotYetImplemented("target different than native");

		match_optional(target_machine.toOpt()) {
			opt_some(target_machine_ref) {
				if (target_machine_ref->getTargetTriple().getTriple() == target_triple)
					return target_machine_ref;
				else
					CORE_PANIC(
						"LLVM error: target machine already initialized with different target"
					);
			}
			opt_none {
				std::string error;
				auto        target = llvm::TargetRegistry::lookupTarget(target_triple, error);

				// Error if we couldn't find the requested target.
				if (!target) CORE_PANIC("LLVM error: " + error);

				auto cpu      = "generic";
				auto features = "";

				llvm::TargetOptions opt;
				this->target_machine
					= Box<llvm::TargetMachine>::fromPointer(target->createTargetMachine(
						target_triple,
						cpu,
						features,
						opt,
						llvm::Reloc::PIC_,
						std::nullopt,
						toLLVMCodeGenOptLevel(
							global_state::getBackendOptions()->llvm_backend->llvm_optimization_level
						)
					));
				return this->target_machine.refMut().toOpt().value();
			}
		}
		CORE_UNREACHABLE();
	}
}
