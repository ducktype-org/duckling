/**
 * @file macro_base.hpp
 * @brief This file contains macros that are used in multiple places in the codebase and are not
 * specific to any particular module. These macros are generally used for metaprogramming and code
 * generation. Inspired from:
 * https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
 */
#pragma once

// NOLINTBEGIN(modernize-macro-to-enum,cppcoreguidelines-macro-to-enum)

#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define EMPTY()
#define DEFER(id)     id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()

#define COMMA         ,
#define FIRST(a, ...) a

#define EXPAND(...) __VA_ARGS__
#define PARENS      ()

#define CAT_PRIMITIVE(a, ...) a##__VA_ARGS__

#define COMPL(b) CAT_PRIMITIVE(COMPL_, b)
#define COMPL_0  1
#define COMPL_1  0

#define BITAND(x)   CAT_PRIMITIVE(BITAND_, x)
#define BITAND_0(y) 0
#define BITAND_1(y) y

#define BITOR(x)   CAT_PRIMITIVE(BITOR_, x)
#define BITOR_0(y) y
#define BITOR_1(y) 1

#define BITOR_2(a, b)                                  BITOR(a)(b)
#define BITOR_3(a, b, c)                               BITOR_2(BITOR_2(a, b), c)
#define BITOR_4(a, b, c, d)                            BITOR_2(BITOR_3(a, b, c), d)
#define BITOR_DISPATCH(...)                            BITOR_DISPATCH_IMPL(__VA_ARGS__, BITOR_4, BITOR_3, BITOR_2, EXPAND)
#define BITOR_DISPATCH_IMPL(_1, _2, _3, _4, NAME, ...) NAME

#define BITOR_ALL(...) BITOR_DISPATCH(__VA_ARGS__)(__VA_ARGS__)

#define INC(x) CAT_PRIMITIVE(INC_, x)
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

#define DEC(x) CAT_PRIMITIVE(DEC_, x)
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

#define NOT(x) CHECK(CAT_PRIMITIVE(NOT_, x))
#define NOT_0  PROBE(~)

#define IIF(c)        CAT_PRIMITIVE(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define BOOL(x)     COMPL(NOT(x))
#define CONST_IF(c) IIF(BOOL(c))

#define EAT(...)

/**
 * @brief If that can be used in macros
 */
#define IF(c) IIF(BOOL(c))

// NOLINTEND(modernize-macro-to-enum,cppcoreguidelines-macro-to-enum)
