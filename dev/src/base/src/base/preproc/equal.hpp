#pragma once
#include "macro_base.hpp"

/**
 * @brief For a token to be "compareable" it needs to be defined as a macro that takes one argument
 * and expands to that argument.
 * For example:
 * #define COMPARE_foo(x) x
 * #define COMPARE_bar(x) x
 */
#define IS_COMPARABLE(x) IS_PAREN(CAT_PRIMITIVE(COMPARE_, x)(()))

#define PRIMITIVE_COMPARE(x, y) IS_PAREN(COMPARE_##x(COMPARE_##y)(()))

#define NOT_EQUAL(x, y) \
	IIF(BITAND(IS_COMPARABLE(x))(IS_COMPARABLE(y)))(PRIMITIVE_COMPARE, 1 EAT)(x, y)

/**
 * @brief Compares 2 tokens.
 * @return 1 if the tokens are equal, 0 otherwise.
 * Example usage:
 * #define COMPARE_foo(x) x
 * #define COMPARE_bar(x) x
 * EQUAL(foo, foo) // Expands to 1
 * IF(EQUAL(foo, bar))(printf("Equal"), printf("Not equal")) // Prints "Not equal"
 *
 */
#define EQUAL(x, y) COMPL(NOT_EQUAL(x, y))
