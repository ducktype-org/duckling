// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

LLVM_INCLUDE_END()

#include <abi/type_system/type.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Translates an ABI type (as produced by the calling-convention library) into the
	 * corresponding llvm::Type.
	 *
	 * Unlike @ref typeFromLayout, this works on @ref abi::types::AbiType — the coerced,
	 * ABI-level shape a value takes when crossing a C ABI boundary (e.g. `{ i64, i64 }` for a
	 * 16-byte struct passed in two registers). The width/shape is intrinsic to the ABI type, so
	 * no module data layout is needed; only the context to allocate the LLVM types.
	 *
	 * @param context The LLVM context to create the types in.
	 * @param type The ABI type to translate.
	 * @return The created llvm::Type*.
	 */
	llvm::Type* abiTypeToLLVMType(llvm::LLVMContext& context, const abi::types::AbiType& type);
}
