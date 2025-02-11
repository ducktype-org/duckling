#pragma once

#include "module_impl.hpp"
#include <lir/lir_structure/lir_structure.hpp>  // @TODO #404 relax it

namespace compiler::backend_llvm {
	auto initModuleImpl(base::StrID module_id) -> Box<ModuleImpl>;

	void addFunctionToModule(Ref<llvm::Module> module, CRef<lir::Function> lir_function);
}
