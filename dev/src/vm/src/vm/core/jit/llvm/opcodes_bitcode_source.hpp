/**
 * @file opcodes_bitcode_source.hpp
 * @brief Provides API to get llvm:function for each microopcode.
 * @details Wrapped in ENABLE_JIT macro to break llvm dependency.
 */
#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <base/collections/optional.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
LLVM_INCLUDE_END()

/**
 * @brief Returns the name of the corresponding LLVM function for the given opcode.
 * @note Required because LLVM modules "disappear" upon materialization.
 */
base::Optional<std::string> llvmGetFunName(const vm::low::MicroOpcode& fun);

/**
 * @brief Returns true if the opcode should not be invoked (e.g. `ext` opcodes).
 */
bool isOpcodeNonExecutable(const vm::low::MicroOpcode& fun);

/**
 * @brief Returns the ThreadSafeContext instance.
 */
Ref<llvm::orc::ThreadSafeContext> llvmGetTSCtx();

/**
 * @brief Returns LLJIT instance.
 * @note For now it is stored in opcodes_bitcode_source but it will change.
 */
Ref<llvm::orc::LLJIT> llvmGetLljit();

/**
 * @brief Returns a pointer to the master IR cache module.
 */
Ref<llvm::Module> llvmGetMasterModule();

Ref<llvm::StructType> llvmGetFrameType();

Ref<llvm::StructType> llvmGetFlagDataType();

Ref<llvm::FunctionType> llvmGetOpFunType();
