#pragma once

#include "llvm_includes/module.hpp"
#include <lir/lir_structure/lir_structure.hpp>  // @TODO #404 relax it

namespace compiler::backend_llvm {
	auto initLLVMModule(base::StrID module_id) -> Box<llvm::Module>;

	void addFunctionToLLVMModule(Ref<llvm::Module> module, CRef<lir::Function> lir_function);
}
