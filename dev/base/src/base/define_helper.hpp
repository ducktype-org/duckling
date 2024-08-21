/**
 * @file define_helper.hpp
 *
 * @brief Define helper is a set of functionalities commonly used in macro programming.
 *
 * If-s macros allow for simple conditional compilation. `IF(true, A, B)` will expand o `A`,
`IF(false, A, B)` will expand to `B`. Analogously for `IF_NOT`.
 *
 * Push/pop diagnostics allows to push/pop diagnostic options via pragmas with acts like diagnostic
scope. If a diagnostic option is changed using `_Pragma` inside push/pop pair, it will only affect
code inside this pair.
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

#include <vector>
#include <string>

namespace base {
	/**
	 * @brief Divides a list of arguments divided by commas into separate strings while ignoring any
	 * white spaces.
	 *
	 * @note This solution is somewhat over engineered.
	 */
	std::vector<std::string> vaArgSplit(std::string_view va_arg);
}

#define CONCAT(arg1, arg2) arg1##arg2
/**
 * @brief Combines arguments after expanding them.
 *
 * @note This is needed so arg1, arg2 will be expanded @n See:
 * https://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html
 */
#define CONCAT_2(arg1, arg2) CONCAT(arg1, arg2)

/**
 * @brief This is useful when dealing with template arguments inside macros
 * Only () "safeguards" commas, [] and <> does not
 * see `balance` on: https://gcc.gnu.org/onlinedocs/cpp/Macro-Arguments.html
 *
 * That means that: SOME_MACRO(type<int, int>) will receive two arguments:
 * `type<int` and `int>`
 *
 * This can be avoided using: SOME_MACRO(type<int COMMA int>)
 */
#define COMMA ,

/**
 * @brief If that can be used in macros
 */
#define IF(cond, t, e) CONCAT(IF_, cond)(t, e)
#define IF_false(t, e) e
#define IF_true(t, e)  t

#define IF_NOT(cond, t, e) CONCAT(IF_NOT, cond)(t, e)
#define IF_NOT_false(t, e) t
#define IF_NOT_true(t, e)  e

#if defined(__clang__)
	#define PUSH_DIAGNOSTIC _Pragma("clang diagnostic push")
	#define NO_SHADOW       _Pragma("clang diagnostic ignored \"-Wshadow-all\"")
	#define POP_DIAGNOSTIC  _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
	#define PUSH_DIAGNOSTIC _Pragma("GCC diagnostic push")
	#define NO_SHADOW                                        \
		_Pragma("GCC diagnostic ignored \"-Wshadow=local\"") \
			_Pragma("GCC diagnostic ignored \"-Wshadow=compatible-local\"")
	#define POP_DIAGNOSTIC _Pragma("GCC diagnostic pop")
#endif
