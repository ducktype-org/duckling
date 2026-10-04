#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

LLVM_INCLUDE_END()

/**
 * @brief Registers absolute symbols for non-jittable opfunctions to work.
 * @details Absolute symbols are constants used by jit, that origin from VM.
 * For example addresses of VM functions or variables (not currently).
 * In most cases jit uses functions which are created and independent from
 * its VM version (they correspond to same function, just stored in llvm).
 */
void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit);
