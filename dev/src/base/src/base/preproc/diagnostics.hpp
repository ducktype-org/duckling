/**
 * @file diagnostics.hpp
 *
 * @brief Push/pop diagnostics allows to push/pop diagnostic options via pragmas with acts like
 diagnostic scope.
 * If a diagnostic option is changed using `_Pragma` inside push/pop pair, it will only affect
 * code inside this pair.
 *
 * @note Doxygen does not see those macros for some reason. Probably because they are inside if-s.
 *
 * ### Usage:
 * @code
    PUSH_DIAGNOSTIC
    NO_SHADOW
    // shadowed declarations are ignored here
    POP_DIAGNOSTIC
 * @endcode
 */
#pragma once

#if defined(__clang__)
	#define PUSH_DIAGNOSTIC _Pragma("clang diagnostic push")
	#define NO_SHADOW       _Pragma("clang diagnostic ignored \"-Wshadow-all\"")
	#define UNHANDLED_ENUM  _Pragma("clang diagnostic error \"-Wswitch\"")
	#define ALLOW_EXTENSIONS                                     \
		_Pragma("clang diagnostic ignored \"-Wc23-extensions\"") \
			_Pragma("clang diagnostic ignored \"-Wc++26-extensions\"")
	#define IGNORE_ASSUME  _Pragma("clang diagnostic ignored \"-Wassume\"")
	#define POP_DIAGNOSTIC _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
	#define PUSH_DIAGNOSTIC _Pragma("GCC diagnostic push")
	#define NO_SHADOW                                        \
		_Pragma("GCC diagnostic ignored \"-Wshadow=local\"") \
			_Pragma("GCC diagnostic ignored \"-Wshadow=compatible-local\"")
	#define UNHANDLED_ENUM _Pragma("GCC diagnostic error \"-Wswitch\"")
	#define ALLOW_EXTENSIONS                                   \
		_Pragma("GCC diagnostic ignored \"-Wc23-extensions\"") \
			_Pragma("GCC diagnostic ignored \"-Wc++26-extensions\"")
	#define IGNORE_ASSUME  _Pragma("GCC diagnostic ignored \"-Wassume\"")
	#define POP_DIAGNOSTIC _Pragma("GCC diagnostic pop")
#endif
