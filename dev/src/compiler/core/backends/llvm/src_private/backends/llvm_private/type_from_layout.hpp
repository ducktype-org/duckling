// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

LLVM_INCLUDE_END()

#include <tsl/type_layout.hpp>

#include <base/pointers/ref.hpp>

namespace compiler::backend_llvm {

	/**
	 * @brief Converts a TypeLayout into its corresponding llvm::Type representation.
	 *
	 * Defined in llvm_lowering.cpp. Declared here so the call-lowering translation
	 * unit can reuse it without pulling in the whole lowering machinery.
	 *
	 * @param module The LLVM module to get the LLVM Context and Module DataLayout.
	 * @param layout The TypeLayout to convert.
	 * @return The created llvm::Type*.
	 */
	llvm::Type* typeFromLayout(Ref<llvm::Module> module, CRef<tsl::TypeLayout> layout);
}
