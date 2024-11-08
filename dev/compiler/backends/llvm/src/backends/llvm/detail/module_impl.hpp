#pragma once

#include <base/box.hpp>
#include "llvm_includes/module.hpp"

namespace compiler::lir {
	struct ModuleImpl {
		Box<llvm::Module> module;

		[[nodiscard]]
		bool verify() const;
		
	};
}
