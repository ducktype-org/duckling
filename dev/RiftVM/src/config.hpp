/**
 * @file config.hpp
 * @brief Configuration macors for the VM.
 *
 * There are 3 different `Executor` implementations:
 * - Tail calls
 * - Switch case
 * - Computed goto
 *
 * They differ in the way they handle the main loop of the `Executor`
 * and they use different type for the "instrucion struct" `Fix8Instruction`.
 * More information in the paper 
 * ["Nowoczesne metody optymalizacji..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/pondvm-opt-pl.pdf)
 */

#pragma once

// Default = false
constexpr bool IGNORE_EXECUTION_STRATEGY = false;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// WARN: Do not undefine this macro on your own!
// Selecting SC or CG will unselect it for you
#define USE_TAIL_CALLS

#if !defined(USE_TAIL_CALLS)
	#error "USE_TAIL_CALLS: Do NOT undefine me like this!"
#endif

// #define USE_SWITCH_CASE

// #define USE_COMPUTED_GOTO

#if defined(USE_SWITCH_CASE) || defined(USE_COMPUTED_GOTO)
	#undef USE_TAIL_CALLS
#endif

#if defined(USE_SWITCH_CASE) && defined(USE_COMPUTED_GOTO)
	#error "USE_SWITCH_CASE and USE_COMPUTED_GOTO are mutually exclusive. Choose only one"
#endif

#ifdef USE_COMPUTED_GOTO
	#define IF_NOT_CG(arg)
	#define IF_CG(arg) arg
constexpr bool USE_COMPUTED_GOTO_VALUE = true;
#else
	#define IF_NOT_CG(arg) arg
	#define IF_CG(arg)
constexpr bool USE_COMPUTED_GOTO_VALUE = false;
#endif

#ifdef USE_TAIL_CALLS
	#define IF_NOT_TC(arg)
	#define IF_TC(arg) arg
constexpr bool USE_TAIL_CALLS_VALUE = true;
#else
	#define IF_NOT_TC(arg) arg
	#define IF_TC(arg)
constexpr bool USE_TAIL_CALLS_VALUE = false;
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
