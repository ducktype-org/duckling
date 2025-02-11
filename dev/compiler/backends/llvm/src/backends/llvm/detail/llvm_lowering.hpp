#pragma once

#include "module_impl.hpp"
#include <lir/lir_structure/lir_structure.hpp>

namespace compiler::backend_llvm {
	auto initModule(base::StrID module_id) -> Box<ModuleImpl>;

	void addFunctionToModule(Ref<llvm::Module> module, CRef<lir::Function> lir_function);
}
