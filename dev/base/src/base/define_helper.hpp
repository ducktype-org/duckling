#pragma once

#include <vector>
#include <string>

namespace base {
	/**
	 * @brief Divides a list of arguments divided by commas into separate strings while ignoring any white spaces.
	 * 
	 * @note This solution is somewhat over engineered.
	 */
	std::vector<std::string> vaArgSplit(std::string_view va_arg);
}

#define CONCAT(arg1, arg2) arg1##arg2
/**
 * @brief Combines arguments after expanding them.
 * 
 * @note This is needed so arg1, arg2 will be expanded @n See: https://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html
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
 * @brief If that can be used in 
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
