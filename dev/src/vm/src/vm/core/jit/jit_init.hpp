/**
 * @file jit_init.hpp
 * @brief The JIT compiler API for initializing. (?)
 */
#pragma once

/**
 * @brief Initializes JIT C2 compiler.
 * @note It is implemented in opcodes_bitcode_source.cpp.
 */
__attribute__((noinline)) void llvmInit();

#ifdef ENABLE_JIT
	#include <init/init.hpp>

RUN_BEFORE_MAIN(init::registerForInit(llvmInit));
#endif
