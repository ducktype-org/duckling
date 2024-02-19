#pragma once

// Default = false
constexpr bool IGNORE_EXECUTION_STRATEGY = false;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define USE_COMPUTED_GOTO

// #define USE_FLAT_FRAME

// #define USE_TAIL_CALLS

#ifdef USE_COMPUTED_GOTO
	#define IF_NOT_CG(arg)
	#define IF_CG(arg) arg
constexpr bool USE_COMPUTED_GOTO_VALUE = true;
#else
	#define IF_NOT_CG(arg) arg
	#define IF_CG(arg)
constexpr bool USE_COMPUTED_GOTO_VALUE = false;
#endif

#ifdef USE_FLAT_FRAME
	#define IF_NOT_FF(arg)
	#define IF_FF(arg) arg
constexpr bool USE_FLAT_FRAME_VALUE = true;
#else
	#define IF_NOT_FF(arg) arg
	#define IF_FF(arg)
constexpr bool USE_FLAT_FRAME_VALUE = false;
#endif

#ifdef USE_TAIL_CALLS
	#define IF_NOT_TC(arg)
	#define IF_TC(arg) arg
constexpr bool USE_TAIL_CALLS_VALUE = true;
#else
	#define IF_NOT_TC(arg) arg
	#define IF_TC(arg)
constexpr bool USE_TAIL_CALLS_VALUE = false;

// NOLINTEND(cppcoreguidelines-macro-usage)

#endif
