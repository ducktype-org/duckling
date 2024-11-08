#pragma once

#include <base/box.hpp>
#include "llvm_includes/module.hpp"

namespace compiler::backend_llvm {
	struct ModuleImpl {
		Box<llvm::Module> module;

		ModuleImpl(Box<llvm::Module> module): module(std::move(module)) {}

		[[nodiscard]]
		bool verify() const;
	};
}
