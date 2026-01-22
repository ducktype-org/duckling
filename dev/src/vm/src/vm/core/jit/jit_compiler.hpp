/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>  // maybe remove this dependency?

namespace vm {
    using JitOpFun = void(vm::MicroInstruction const**, std::byte**, vm::Frame**, vm::VMThread*);
}

vm::JitOpFun* compileJit(const vm::low::LowFuncData& func_data);

