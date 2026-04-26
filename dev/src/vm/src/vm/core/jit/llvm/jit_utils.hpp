#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

/**
 * @brief Creates a new empty llvm::Module with the same data layout and target triple as in the
 * master module.
 */
std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext& ctx);

/**
 * @brief Returns whether opcode is not executable like ext_*.
 */
bool isOpcodeNonExecutable(const vm::low::MicroOpcode& opcode);