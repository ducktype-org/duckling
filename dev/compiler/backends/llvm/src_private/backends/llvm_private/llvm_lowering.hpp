#pragma once

#include <lir/lir_structure/lir_structure.hpp>  // @TODO #404 relax it
#include <query_framework/query_impl.hpp>  // @TODO #404 relax it to just context

namespace compiler::backend_llvm {
	struct ModuleImpl;

	Box<ModuleImpl> initModuleImpl(base::StrID module_id);

	void addFunctionToModuleImpl(query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function);
}
