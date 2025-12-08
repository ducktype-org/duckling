#pragma once

#include <base/pointers/box.hpp>

namespace compiler::backend_llvm {
	struct ModuleImpl;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(compiler::backend_llvm::ModuleImpl)
