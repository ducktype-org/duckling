#pragma once

#include "llvm_includes/module.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include "llvm/IR/LLVMContext.h"

LLVM_INCLUDE_BEGIN()
#include <llvm/Target/TargetMachine.h>
LLVM_INCLUDE_END()

#include <base/box.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module.
	 * Implements it is a way similar to pimpl idiom.
	 */
	struct ModuleImpl {
		/**
		 * One LLVMContext per module means that there can be multiple modules created and compiled concurently.
		 * https://llvm.org/docs/ProgrammersManual.html#achieving-isolation-with-llvmcontext
		 * https://llvm.org/doxygen/classllvm_1_1LLVMContext.html#details
	 	 * Single context can't be used my multiple threads.
		 * The context should deallocate after the module. 
		 */
		Box<llvm::LLVMContext>    context;
		Box<llvm::Module>         module;
		MBox<llvm::TargetMachine> target_machine;

		ModuleImpl(Box<llvm::LLVMContext> context, Box<llvm::Module> module):
			  context(std::move(context)),
			  module(std::move(module)) {}

		~ModuleImpl() = default;

		Ref<llvm::TargetMachine> setTargetMachine(const std::string& target_triple);

		friend struct Module;
	};
}
