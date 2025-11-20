#pragma once

#include "llvm_includes/module.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Target/TargetMachine.h>
LLVM_INCLUDE_END()

#include <base/pointers/box.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module.
	 * Implements it is a way similar to pimpl idiom.
	 */
	struct ModuleImpl final {
		Box<llvm::Module>         module;
		MBox<llvm::TargetMachine> target_machine;

		explicit ModuleImpl(Box<llvm::Module> module);

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
