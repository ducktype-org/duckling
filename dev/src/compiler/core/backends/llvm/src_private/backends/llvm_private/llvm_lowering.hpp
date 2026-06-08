#pragma once

#include <lir/lir_structure/lir_structure.hpp>  // @TODO: #404 relax it

#include <query_framework/context/context_fd.hpp>

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
	 * @param llvm_bc_data The LLVM bitcode data.
	 * @return Box<ModuleImpl> The created LLVM module object.
	 */
	Box<ModuleImpl> parseLLVMBCToModuleImpl(std::span<unsigned char> llvm_bc_data);

	void addFunctionToModuleImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addFunctionToModuleCtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addFunctionToModuleDtorsImpl(
		query::Context& ctx, Ref<ModuleImpl> module, CRef<lir::Function> lir_function
	);

	void addGlobalDeclarationToModuleImpl(
		Ref<ModuleImpl> module, const lir::LIRGlobalData& lir_global
	);

	void setGlobalConstantInitializerImpl(
		Ref<ModuleImpl> module, base::StrID global_name, const ctv::CompileTimeValue& constant_value
	);
}
