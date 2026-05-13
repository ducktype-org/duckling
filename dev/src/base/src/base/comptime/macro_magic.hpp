/**
 * @file macro_magic.hpp
 * @brief This file contains macros that are used in multiple places in the codebase and are not
 * specific to any particular module. These macros are generally used for metaprogramming and code
 * generation. Inspired from:
 * https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
 */

#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()

#define EXPAND(...) __VA_ARGS__
#define PAREN(...) (__VA_ARGS__)
#define PARENS ()

#define CAT_PRIMITIVE(a, b) a ## b

#define CAT_1(a) a
#define CAT_2(a, b) CAT_PRIMITIVE(a, b)
#define CAT_3(a, b, c) CAT_2(CAT_2(a, b), c)
#define CAT_4(a, b, c, d) CAT_2(CAT_3(a, b, c), d)
#define CAT_5(a, b, c, d, e) CAT_2(CAT_4(a, b, c, d), e)
#define CAT_6(a, b, c, d, e, f) CAT_2(CAT_5(a, b, c, d, e), f)
#define CAT_7(a, b, c, d, e, f, g) CAT_2(CAT_6(a, b, c, d, e, f), g)
#define CAT_8(a, b, c, d, e, f, g, h) CAT_2(CAT_7(a, b, c, d, e, f, g), h)
#define CAT_9(a, b, c, d, e, f, g, h, i) CAT_2(CAT_8(a, b, c, d, e, f, g, h), i)
#define CAT_10(a, b, c, d, e, f, g, h, i, j) CAT_2(CAT_9(a, b, c, d, e, f, g, h, i), j)
#define CAT_11(a, b, c, d, e, f, g, h, i, j, k) CAT_2(CAT_10(a, b, c, d, e, f, g, h, i, j), k)
#define CAT_12(a, b, c, d, e, f, g, h, i, j, k, l) CAT_2(CAT_11(a, b, c, d, e, f, g, h, i, j, k), l)
#define CAT_13(a, b, c, d, e, f, g, h, i, j, k, l, m) CAT_2(CAT_12(a, b, c, d, e, f, g, h, i, j, k, l), m)
#define CAT_14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) CAT_2(CAT_13(a, b, c, d, e, f, g, h, i, j, k, l, m), n)
#define CAT_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) CAT_2(CAT_14(a, b, c, d, e, f, g, h, i, j, k, l, m, n), o)
#define CAT_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) CAT_2(CAT_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o), p)

#define CAT_DISPATCH_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16, MACRO, ...) MACRO
#define CAT_DISPATCH(...) CAT_DISPATCH_IMPL(__VA_ARGS__, CAT_16, CAT_15, CAT_14, CAT_13, CAT_12, CAT_11, CAT_10, CAT_9, CAT_8, CAT_7, CAT_6, CAT_5, CAT_4, CAT_3, CAT_2, CAT_1)

#define CAT(...) CAT_DISPATCH(__VA_ARGS__)(__VA_ARGS__)

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0  1
#define COMPL_1  0

#define BITAND(x)   PRIMITIVE_CAT(BITAND_, x)
#define BITAND_0(y) 0
#define BITAND_1(y) y

#define INC(x) PRIMITIVE_CAT(INC_, x)
#define INC_0  1
#define INC_1  2
#define INC_2  3
#define INC_3  4
#define INC_4  5
#define INC_5  6
#define INC_6  7
#define INC_7  8
#define INC_8  9
#define INC_9  9

#define DEC(x) PRIMITIVE_CAT(DEC_, x)
#define DEC_0  0
#define DEC_1  0
#define DEC_2  1
#define DEC_3  2
#define DEC_4  3
#define DEC_5  4
#define DEC_6  5
#define DEC_7  6
#define DEC_8  7
#define DEC_9  8

#define CHECK_N(x, n, ...) n
#define CHECK(...)         CHECK_N(__VA_ARGS__, 0, )
#define PROBE(x)           x, 1,

/**
 * @brief Checks if the given token is a parenthesis. This is useful for detecting empty __VA_ARGS__
 * and for other metaprogramming tricks.
 */
#define IS_PAREN(x)         CHECK(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0  PROBE(~)

#define IIF(c) PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define BOOL(x) COMPL(NOT(x))
#define CONST_IF(c)   IIF(BOOL(c))


#define EAT(...)
#define WHEN(c)     IF(c)(EXPAND, EAT)


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

/**
 * @brief For a token to be "compareable" it needs to be defined as a macro that takes one argument
 * and expands to that argument.
 * For example:
 * #define COMPARE_foo(x) x
 * #define COMPARE_bar(x) x
 */
#define IS_COMPARABLE(x) IS_PAREN(CAT(COMPARE_, x)(()))

#define NOT_EQUAL(x, y) \
	IIF(BITAND(IS_COMPARABLE(x))(IS_COMPARABLE(y)))(PRIMITIVE_COMPARE, 1 EAT)(x, y)

/**
 * @brief Compares 2 tokens.
 * @return 1 if the tokens are equal, 0 otherwise.
 * Example usage:
 * #define COMPARE_foo(x) x
 * #define COMPARE_bar(x) x
 *
 */
#define EQUAL(x, y) COMPL(NOT_EQUAL(x, y))
