#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/low_program/opcodes.hpp>
LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
LLVM_INCLUDE_END()

llvm::Function*   llvm_get_fun(const vm::low::MicroOpcode& fun);
llvm::orc::LLJIT* llvm_get_lljit();
