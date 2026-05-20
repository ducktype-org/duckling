#pragma once
// Heavily inspired by (actually copied from): https://www.scs.stanford.edu/~dm/blog/va-opt.html

#define PARENS        ()
#define FIRST(a, ...) a

#define AUX_EXPAND0(...) AUX_EXPAND4(AUX_EXPAND4(AUX_EXPAND4(AUX_EXPAND4(__VA_ARGS__))))
#define AUX_EXPAND4(...) AUX_EXPAND3(AUX_EXPAND3(AUX_EXPAND3(AUX_EXPAND3(__VA_ARGS__))))
#define AUX_EXPAND3(...) AUX_EXPAND2(AUX_EXPAND2(AUX_EXPAND2(AUX_EXPAND2(__VA_ARGS__))))
#define AUX_EXPAND2(...) AUX_EXPAND1(AUX_EXPAND1(AUX_EXPAND1(AUX_EXPAND1(__VA_ARGS__))))
#define AUX_EXPAND1(...) __VA_ARGS__

/**
 * @brief Macro that applies `macro` on all arguments.
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
#define FOR_EACH(macro, ...) __VA_OPT__(AUX_EXPAND0(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
	macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

/**
 * @brief For each macro, but gives the called macro an `arg` argument + iterates over the rest.
 */
#define FOR_EACH_ARG(macro, arg, ...) \
	__VA_OPT__(AUX_EXPAND0(FOR_EACH_HELPER_ARG(macro, arg, __VA_ARGS__)))
#define FOR_EACH_HELPER_ARG(macro, arg, a1, ...) \
	macro(arg, a1) __VA_OPT__(FOR_EACH_AGAIN_ARG PARENS(macro, arg, __VA_ARGS__))
#define FOR_EACH_AGAIN_ARG() FOR_EACH_HELPER_ARG

/**
 * @brief For each macro, but gives the called macro `arg0` and `arg1` arguments + iterates over the
 * rest.
 */
#define FOR_EACH_2ARG(macro, arg0, arg1, ...) \
	__VA_OPT__(AUX_EXPAND0(FOR_EACH_HELPER_2ARG(macro, arg0, arg1, __VA_ARGS__)))
#define FOR_EACH_HELPER_2ARG(macro, arg0, arg1, a1, ...) \
	macro(arg0, arg1, a1) __VA_OPT__(FOR_EACH_AGAIN_2ARG PARENS(macro, arg0, arg1, __VA_ARGS__))
#define FOR_EACH_AGAIN_2ARG() FOR_EACH_HELPER_2ARG

/**
 * @brief For each macro, but has a separate `last_elem_macro` that is called for the last element
 * instead of `macro`.
 */
#define FOR_EACH_CUSTOM_LAST(macro, last_elem_macro, ...) \
	__VA_OPT__(AUX_EXPAND0(FOR_EACH_CUSTOM_LAST_HELPER(macro, last_elem_macro, __VA_ARGS__)))
#define FOR_EACH_CUSTOM_LAST_HELPER(macro, last_elem_macro, a1, ...) \
	FIRST(__VA_OPT__(macro, ) last_elem_macro)(a1)                   \
		__VA_OPT__(FOR_EACH_CUSTOM_LAST_AGAIN PARENS(macro, last_elem_macro, __VA_ARGS__))
#define FOR_EACH_CUSTOM_LAST_AGAIN() FOR_EACH_CUSTOM_LAST_HELPER
