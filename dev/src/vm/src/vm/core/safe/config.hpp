/**
 * @file config.hpp
 * @brief Configuration macros for the VM.
 *
 * There are 2 different `Executor` implementations:
 * - Tail calls
 * - Switch case
 *
 * They differ in the way they handle the main loop of the `Executor`
 * and they use different type for the "instruction struct" `MicroInstruction`.
 */

#pragma once

// Default = false
constexpr bool IGNORE_EXECUTION_STRATEGY = false;

#if !(defined(USE_TAIL_CALLS) || defined(USE_SWITCH_CASE))
	#error "Provide an execution strategy: USE_TAIL_CALLS or USE_SWITCH_CASE"
#endif

#if defined(USE_TAIL_CALLS) && defined(USE_SWITCH_CASE)
	#error "Cannot use both USE_TAIL_CALLS and USE_SWITCH_CASE"
#endif

#ifdef USE_TAIL_CALLS
	#define IF_NOT_TC(arg)
	#define IF_TC(arg) arg
inline constexpr bool USE_TAIL_CALLS_VALUE  = true;
inline constexpr bool USE_SWITCH_CASE_VALUE = false;
#else
	#define IF_NOT_TC(arg) arg
	#define IF_TC(arg)
inline constexpr bool USE_TAIL_CALLS_VALUE  = false;
inline constexpr bool USE_SWITCH_CASE_VALUE = true;
#endif
