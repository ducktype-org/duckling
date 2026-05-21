#pragma once

#define WHEN(c) IF(c)(EXPAND, EAT)

/**
 * @brief Repeats a macro a specified number of times.
 * @param count The number of times to repeat the macro.
 * @param macro The macro to repeat.
 * @param ... Additional arguments to pass to the macro.
 * Example usage:
 * #define PRINT_NUM(n, _) printf("%d\n", n);
 * REPEAT(5, PRINT_NUM, ~) // This will print numbers from 0 to 4, each on a new line.
 */
#define REPEAT(count, macro, ...)                                                            \
	WHEN(count)(OBSTRUCT(REPEAT_INDIRECT)()(DEC(count), macro, __VA_ARGS__) OBSTRUCT(macro)( \
		DEC(count), __VA_ARGS__                                                              \
	))
#define REPEAT_INDIRECT() REPEAT

#define WHILE(pred, op, ...) \
	IF(pred(__VA_ARGS__))(OBSTRUCT(WHILE_INDIRECT)()(pred, op, op(__VA_ARGS__)), __VA_ARGS__)
#define WHILE_INDIRECT() WHILE
