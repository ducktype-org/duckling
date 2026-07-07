#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/pointers/ref.hpp>

#include <vector>

namespace compiler::backend_llvm {

	/**
	 * @brief Get the LLVM function type based on the layouts of its parameters and return type.
	 *
	 * @param module The LLVM module in which the function type will be used.
	 * @param parameters The layouts of the parameters of the function.
	 * @param return_type The layout of the return type of the function.
	 * @param abi The ABI to conform to.
	 * @return The LLVM function type.
	 */
	llvm::FunctionType* getFunType(
		Ref<llvm::Module>                         module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		CRef<tsl::TypeLayout>                     return_type,
		const lir::LIRAbi&                        abi = lir::LIRAbi{ lir::LIRAbi::DefaultAbi{} }
	);

	/**
	 * @brief Maps a LIR ABI to the LLVM calling convention to use for the function.
	 */
	llvm::CallingConv::ID getCallingConvFromABI(const lir::LIRAbi& abi);

	/**
	 * @brief Gets a function from a module by the function literal (using its mangled_name field).
	 *
	 * If the function doesn't exist it adds a function prototype with external linkage to the
	 * module based on the provided function literal.
	 */
	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLiteral(
		Ref<llvm::Module> module, const lir::FunctionLiteral& function_literal
	);

	/**
	 * @brief Same as @ref getOrInsertFunctionPrototypeFromLiteral, but takes a LIR function.
	 */
	llvm::FunctionCallee getOrInsertFunctionPrototypeFromLIRFunction(
		Ref<llvm::Module> module, const lir::Function& lir_function
	);
}
