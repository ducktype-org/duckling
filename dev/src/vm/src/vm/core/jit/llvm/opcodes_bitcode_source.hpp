// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> parseOpcodesBitcode(llvm::LLVMContext& context);
