/**
 * @file jit_init.hpp
 * @brief The JIT compiler API for initializing. (?)
 */
#pragma once

__attribute__((noinline)) void llvmInit();

#ifdef ENABLE_JIT
#include <init/init.hpp>

RUN_BEFORE_MAIN(init::registerForInit(llvmInit));
#endif