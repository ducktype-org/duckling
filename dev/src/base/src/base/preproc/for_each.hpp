// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "macro_base.hpp"

// Heavily inspired by (actually copied from): https://www.scs.stanford.edu/~dm/blog/va-opt.html

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
#define FOR_EACH(macro, ...) __VA_OPT__(EVAL(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
	macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

/**
 * @brief For each macro, but separates the results with commas, so the expansion is usable as an
 * argument list.
 *
 * @example:
 * ```cpp
 * #define STRINGIFY_ARG(x) #x
 * const char* names[] = { FOR_EACH_COMMA(STRINGIFY_ARG, a, b, c) };
 * ```
 * expands the initializer to:
 * ```cpp
 * { "a", "b", "c" }
 * ```
 */
#define FOR_EACH_COMMA(macro, ...) __VA_OPT__(EVAL(FOR_EACH_COMMA_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_COMMA_HELPER(macro, a1, ...) \
	macro(a1) __VA_OPT__(COMMA FOR_EACH_COMMA_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_COMMA_AGAIN() FOR_EACH_COMMA_HELPER

/**
 * @brief For each macro, but gives the called macro an `arg` argument + iterates over the rest.
 */
#define FOR_EACH_ARG(macro, arg, ...) __VA_OPT__(EVAL(FOR_EACH_HELPER_ARG(macro, arg, __VA_ARGS__)))
#define FOR_EACH_HELPER_ARG(macro, arg, a1, ...) \
	macro(arg, a1) __VA_OPT__(FOR_EACH_AGAIN_ARG PARENS(macro, arg, __VA_ARGS__))
#define FOR_EACH_AGAIN_ARG() FOR_EACH_HELPER_ARG

/**
 * @brief For each macro, but gives the called macro `arg0` and `arg1` arguments + iterates over the
 * rest.
 */
#define FOR_EACH_2ARG(macro, arg0, arg1, ...) \
	__VA_OPT__(EVAL(FOR_EACH_HELPER_2ARG(macro, arg0, arg1, __VA_ARGS__)))
#define FOR_EACH_HELPER_2ARG(macro, arg0, arg1, a1, ...) \
	macro(arg0, arg1, a1) __VA_OPT__(FOR_EACH_AGAIN_2ARG PARENS(macro, arg0, arg1, __VA_ARGS__))
#define FOR_EACH_AGAIN_2ARG() FOR_EACH_HELPER_2ARG

/**
 * @brief For each macro, but has a separate `last_elem_macro` that is called for the last element
 * instead of `macro`.
 */
#define FOR_EACH_CUSTOM_LAST(macro, last_elem_macro, ...) \
	__VA_OPT__(EVAL(FOR_EACH_CUSTOM_LAST_HELPER(macro, last_elem_macro, __VA_ARGS__)))
#define FOR_EACH_CUSTOM_LAST_HELPER(macro, last_elem_macro, a1, ...) \
	FIRST(__VA_OPT__(macro, ) last_elem_macro)(a1)                   \
		__VA_OPT__(FOR_EACH_CUSTOM_LAST_AGAIN PARENS(macro, last_elem_macro, __VA_ARGS__))
#define FOR_EACH_CUSTOM_LAST_AGAIN() FOR_EACH_CUSTOM_LAST_HELPER
