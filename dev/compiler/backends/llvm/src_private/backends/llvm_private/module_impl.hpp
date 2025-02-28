#pragma once

#include "llvm_includes/module.hpp"
#include <base/box.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module.
	 * Implements it is a way similar to pimpl idiom.
	 */
	struct ModuleImpl {
		Box<llvm::Module> module;

		ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {}

		~ModuleImpl() = default;

		friend struct Module;
	};
}
