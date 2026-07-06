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
	 * @note This is where the C/C++ ABI lowering will live: struct-like parameters passed by
	 * pointer with `byval`, register coercion of small aggregates, `sret` returns, etc. That
	 * logic is driven by the calling-convention library (@ref lir::LIRAbi::CAbi holds the
	 * @ref abi::calling_conv::FunctionInfo) and @ref abiTypeToLLVMType. It is not implemented
	 * yet — only the default ABI and the existing string-by-pointer special case are handled.
	 * See:
	 * https://yorickpeterse.com/articles/the-mess-that-is-handling-structure-arguments-and-returns-in-llvm/.
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

	/**
	 * @brief Lowers a `Call` to the callee, emitting the `llvm::CallInst` and its call-site
	 * attributes.
	 *
	 * The caller is responsible for loading the argument values (@p args, in call order,
	 * excluding the callee) and for storing the result. This function owns everything that
	 * happens at the call boundary: resolving the prototype, the C ABI argument marshalling
	 * (currently: strings passed by pointer with `byval`) and attaching the matching call-site
	 * attributes. This is the place to add further ABI call-site attributes.
	 *
	 * @param module The LLVM module the call is emitted into.
	 * @param builder The IRBuilder positioned at the call site.
	 * @param function_literal The callee.
	 * @param args The already-loaded argument values (may be rewritten for `byval` passing).
	 * @param has_output Whether the call's result is used (drives the void-return assertion).
	 * @return The emitted call instruction.
	 */
	llvm::CallInst* lowerCallInstruction(
		Ref<llvm::Module>           module,
		llvm::IRBuilder<>&          builder,
		const lir::FunctionLiteral& function_literal,
		std::vector<llvm::Value*>   args,
		bool                        has_output
	);
}
