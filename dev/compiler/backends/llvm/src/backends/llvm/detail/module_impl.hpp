#pragma once

#include <base/box.hpp>
#include "llvm_includes/module.hpp"

namespace compiler::backend_llvm {

	/**
	 * @brief Helper class of backend_llvm::Module
	 * Implements it is a way similar to pimpl idiom
	 */
	struct ModuleImpl {
		Box<llvm::Module> module;

		ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {}

		[[nodiscard]]
		bool verify() const;

		void debugPrint() const;
	};
}
