#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

LLVM_INCLUDE_END()

/**
 * @brief Registers absolute symbols for unjitable opfunctions to work.
 */
void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit);
