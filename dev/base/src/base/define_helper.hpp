#pragma once

#include <vector>
#include <string>

namespace base {
	std::vector<std::string> vaArgSplit(std::string_view va_arg);
}


#define CONCAT_2_(arg1, arg2) arg1 ## arg2
/**
 * @brief This is needed so arg1, arg2 will be expanded
 * See: https://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html
 */
#define CONCAT_2(arg1, arg2) CONCAT_2_(arg1, arg2)

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
