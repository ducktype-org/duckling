#pragma once

#include <lir/lir_structure/lir_structure.hpp>  // @TODO: #404 relax it

#include <query_framework/context_fd.hpp>

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

	/**
	 * @brief Create a new LLVM module from the LLVM bitcode representation.
	 * The input LLVM bitcode should include a complete module definition.
	 *
	 * @param llvm_bc_data A pointer to the LLVM bitcode data.
	 * @param llvm_bc_size The size of the LLVM bitcode data in bytes.
	 * @return Box<ModuleImpl> The created LLVM module object.
	 */
	Box<ModuleImpl> parseLLVMBCToModuleImpl(const unsigned char* llvm_bc_data, size_t llvm_bc_size);

	void addFunctionToModuleImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addFunctionToModuleCtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addFunctionToModuleDtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addGlobalToModuleImpl(Ref<ModuleImpl> module, const lir::LIRGlobal& lir_global);
}
