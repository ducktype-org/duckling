#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> parseOpcodesBitcode(llvm::LLVMContext& context);
