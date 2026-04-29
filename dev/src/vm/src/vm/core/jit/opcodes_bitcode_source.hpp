/**
 * @file opcodes_bitcode_source.hpp
 * @brief Provides API to get llvm:function for each microopcode.
 * @details Wrapped in ENABLE_JIT macro to break llvm dependency.
 */
#pragma once

#ifdef ENABLE_JIT  // @TODO: #2312 Remove the #ifdef
	#include <llvm_helpers/llvm_helpers.hpp>

	#include <vm/core/safe/low_program/opcodes.hpp>
LLVM_INCLUDE_BEGIN()
	#include <llvm/ExecutionEngine/Orc/LLJIT.h>
	#include <llvm/IR/Function.h>
LLVM_INCLUDE_END()

/**
 * @brief For MicroOpcode returns llvm::Function* of corresponding function.
 */
llvm::Function* llvmGetFun(const vm::low::MicroOpcode& fun);

/**
 * @brief Returns LLJIT instance.
 * @note For now it is stored in opcodes_bitcode_source but it will change.
 */
llvm::orc::LLJIT* llvmGetLljit();

#endif
