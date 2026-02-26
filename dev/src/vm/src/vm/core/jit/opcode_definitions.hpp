#pragma once

#ifdef ENABLE_JIT

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/low_program/opcodes.hpp>
LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
LLVM_INCLUDE_END()

llvm::Function*   llvmGetFun(const vm::low::MicroOpcode& fun);
llvm::orc::LLJIT* llvmGetLljit();

#endif
