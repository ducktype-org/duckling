// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "macro_base.hpp"

/**
 * @brief Repeats a macro a specified number of times.
 * @param count The number of times to repeat the macro.
 * @param macro The macro to repeat.
 * @param ... Additional arguments to pass to the macro.
 * Example usage:
 * #define PRINT_NUM(n) printf("%d\n", n);
 * EVAL(REPEAT(5, PRINT_NUM)) // This will print numbers from 0 to 4, each on a new line.
 */
#define REPEAT(count, macro, ...)                   \
	WHEN(count)(OBSTRUCT(REPEAT_INDIRECT)()(        \
		DEC(count), macro __VA_OPT__(, __VA_ARGS__) \
	) OBSTRUCT(macro)(DEC(count) __VA_OPT__(, __VA_ARGS__)))
#define REPEAT_INDIRECT() REPEAT

/**
 * @brief User-friendly wrapper around WHILE, that allows repeating a macro while a condition is true.
 */
#define WHILE(pred, op, ...)                          \
	IF(pred(__VA_ARGS__))(OBSTRUCT(WHILE_INDIRECT)()( \
		pred, op __VA_OPT__(, op(__VA_ARGS__))        \
	) __VA_OPT__(, __VA_ARGS__))
#define WHILE_INDIRECT() WHILE
