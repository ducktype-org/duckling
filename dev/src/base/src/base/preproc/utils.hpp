/**
 * @file utils.hpp
 *
 * @brief utils is a set of functionalities commonly used in macro programming.
 *
 * If-s macros allow for simple conditional compilation. `IF(true, A, B)` will expand o `A`,
`IF(false, A, B)` will expand to `B`. Analogously for `IF_NOT`.
 */
#pragma once

#define STRINGIFY(arg)   #arg
#define STRINGIFY_2(arg) STRINGIFY(arg)

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
#define IF_THEN_ELSE(cond, t, e) CONCAT(IF_, cond)(t, e)
#define IF_false(t, e) e
#define IF_true(t, e)  t

#define IF_NOT(cond, t, e) CONCAT(IF_NOT, cond)(t, e)
#define IF_NOT_false(t, e) t
#define IF_NOT_true(t, e)  e

#define EXPAND(expr)        expr
#define EXPAND_VA_ARGS(...) __VA_ARGS__
