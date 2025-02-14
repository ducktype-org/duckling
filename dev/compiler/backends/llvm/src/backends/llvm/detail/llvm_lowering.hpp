#pragma once

#include <lir/lir_structure/lir_structure.hpp>  // @TODO #404 relax it

namespace compiler::backend_llvm {
	struct ModuleImpl;

	Box<ModuleImpl> initModuleImpl(base::StrID module_id);

	void addFunctionToModuleImpl(Ref<ModuleImpl> module, CRef<lir::Function> lir_function);
}
