#pragma once
// Heavily inspired by (actually copied): https://www.scs.stanford.edu/~dm/blog/va-opt.html

#define PARENS ()

#define EXPAND(...)  EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

/**
 * @brief Macro that applies `to_apply` on all arguments.
 * @note Number of arguments is currently limited to 30, but it can be easily increased.
 *
 * @example:
 * ```cpp
 * #define MK_STRUCT(name) struct name {};
 # FOR_EACH(MK_STRUCT, S1, S2, S3)
 * ```
 * expands to:
 * ```cpp
 * struct S1 {};
 * struct S2 {};
 * struct S3 {};
 * ```
 */
#define FOR_EACH(macro, ...) __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
	macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

/**
 * @brief For each macro, but gives the called macro an `arg` argument + iterates over the rest.
 */
#define FOR_EACH_ARG(macro, arg, ...) \
	__VA_OPT__(EXPAND(FOR_EACH_HELPER_ARG(macro, arg, __VA_ARGS__)))
#define FOR_EACH_HELPER_ARG(macro, arg, a1, ...) \
	macro(arg, a1) __VA_OPT__(FOR_EACH_AGAIN_ARG PARENS(macro, arg, __VA_ARGS__))
#define FOR_EACH_AGAIN_ARG() FOR_EACH_HELPER_ARG

/**
 * @brief For each macro, but gives the called macro `arg0` and `arg1` arguments + iterates over the
 * rest.
 */
#define FOR_EACH_2ARG(macro, arg0, arg1, ...) \
	__VA_OPT__(EXPAND(FOR_EACH_HELPER_2ARG(macro, arg0, arg1, __VA_ARGS__)))
#define FOR_EACH_HELPER_2ARG(macro, arg0, arg1, a1, ...) \
	macro(arg0, arg1, a1) __VA_OPT__(FOR_EACH_AGAIN_2ARG PARENS(macro, arg0, arg1, __VA_ARGS__))
#define FOR_EACH_AGAIN_2ARG() FOR_EACH_HELPER_2ARG

