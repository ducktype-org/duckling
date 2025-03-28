#pragma once

#include <lir/lir_structure/lir_structure.hpp>  // @TODO #404 relax it

namespace compiler::backend_llvm {
	struct ModuleImpl;

	Box<ModuleImpl> initModuleImpl(base::StrID module_id);

	/**
	 * @brief Create a new LLVM module from the LLVM IR text representation.
	 * The input LLVM IR code should include a complete module definition.

	 * @param llvm_ir_code A string view containing the LLVM IR code.
	 * @return Box<ModuleImpl> The created LLVM module object.
	 */
	Box<ModuleImpl> parseIRCodeToModuleImpl(std::string_view llvm_ir_code);

	void addFunctionToModuleImpl(Ref<ModuleImpl> module, CRef<lir::Function> lir_function);

}
