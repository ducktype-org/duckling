#pragma once

#include "llvm_includes/module.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
LLVM_INCLUDE_END()

#include <backends/llvm/module_impl_fd.hpp>

#include <base/pointers/box.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module.
	 * Implements it is a way similar to pimpl idiom.
	 */
	struct ModuleImpl {
		Box<llvm::Module>         module;
		MBox<llvm::TargetMachine> target_machine;

		explicit ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {
			const std::string target_triple = llvm::sys::getDefaultTargetTriple();
			setTargetMachine(target_triple);
			this->module->setDataLayout(target_machine->createDataLayout());
			this->module->setTargetTriple(target_machine->getTargetTriple().getTriple());
		}

		~ModuleImpl() = default;

		[[nodiscard]]
		MRef<llvm::TargetMachine> getTargetMachine() const {
			return target_machine.refMut();
		}

	private:
		Ref<llvm::TargetMachine> setTargetMachine(const std::string& target_triple);

		friend struct Module;
	};
}
