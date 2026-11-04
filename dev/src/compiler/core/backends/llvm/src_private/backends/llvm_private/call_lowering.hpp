// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/pointers/ref.hpp>

#include <utility>
#include <vector>

namespace compiler::backend_llvm {

	struct LoweredDefaultAbiSignature {
		llvm::FunctionType*                          type;
		bool                                         return_indirect;
		std::vector<bool>                            parameter_is_indirect;
		std::vector<std::pair<u32, llvm::Attribute>> attributes;
	};

	/**
	 * @brief Lowers Duckling's default ABI to an LLVM signature.
	 *
	 * Aggregates larger than two pointer-sized words are kept in memory: parameters become
	 * "ptr byval(T)", while a return value uses a hidden "ptr sret(T)" parameter.
	 */
	LoweredDefaultAbiSignature lowerDefaultAbiSignature(
		Ref<llvm::Module>                         module,
		const std::vector<CRef<tsl::TypeLayout>>& parameters,
		CRef<tsl::TypeLayout>                     return_type
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
