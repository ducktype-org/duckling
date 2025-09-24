#pragma once

#include "llvm_includes/module.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Target/TargetMachine.h>
LLVM_INCLUDE_END()

#include <backends/llvm/module_impl_fd.hpp>

#include <base/box.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module.
	 * Implements it is a way similar to pimpl idiom.
	 */
	struct ModuleImpl {
		Box<llvm::Module>         module;
		MBox<llvm::TargetMachine> target_machine;

		ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {}

		~ModuleImpl() = default;

		Ref<llvm::TargetMachine> setTargetMachine(const std::string& target_triple);

		friend struct Module;
	};
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(compiler::backend_llvm::ModuleImpl)
