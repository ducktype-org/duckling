#pragma once
#include "macro_base.hpp"

#define CAT_1(a)       a
#define CAT_2(a, b)    CAT_PRIMITIVE(a, b)
#define CAT_3(a, ...)  CAT_2(a, CAT_2(__VA_ARGS__))
#define CAT_4(a, ...)  CAT_2(a, CAT_3(__VA_ARGS__))
#define CAT_5(a, ...)  CAT_2(a, CAT_4(__VA_ARGS__))
#define CAT_6(a, ...)  CAT_2(a, CAT_5(__VA_ARGS__))
#define CAT_7(a, ...)  CAT_2(a, CAT_6(__VA_ARGS__))
#define CAT_8(a, ...)  CAT_2(a, CAT_7(__VA_ARGS__))
#define CAT_9(a, ...)  CAT_2(a, CAT_8(__VA_ARGS__))
#define CAT_10(a, ...) CAT_2(a, CAT_9(__VA_ARGS__))
#define CAT_11(a, ...) CAT_2(a, CAT_10(__VA_ARGS__))
#define CAT_12(a, ...) CAT_2(a, CAT_11(__VA_ARGS__))
#define CAT_13(a, ...) CAT_2(a, CAT_12(__VA_ARGS__))
#define CAT_14(a, ...) CAT_2(a, CAT_13(__VA_ARGS__))
#define CAT_15(a, ...) CAT_2(a, CAT_14(__VA_ARGS__))
#define CAT_16(a, ...) CAT_2(a, CAT_15(__VA_ARGS__))

#define CAT_DISPATCH_IMPL(                                                            \
	_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, MACRO, ... \
)                                                                                     \
	MACRO

#define CAT_DISPATCH(...) \
	CAT_DISPATCH_IMPL(    \
		__VA_ARGS__,      \
		CAT_16,           \
		CAT_15,           \
		CAT_14,           \
		CAT_13,           \
		CAT_12,           \
		CAT_11,           \
		CAT_10,           \
		CAT_9,            \
		CAT_8,            \
		CAT_7,            \
		CAT_6,            \
		CAT_5,            \
		CAT_4,            \
		CAT_3,            \
		CAT_2,            \
		CAT_1             \
	)

/**
 * @brief Concatenates tokens after expanding them. This is useful for generating code based on
 * macro arguments.
 * For example, `CAT(foo, bar)` will expand to `foobar`.
 * @note The number of arguments is limited to 16, but this can be easily
 * changed by adding more CAT_N macros and updating the CAT_DISPATCH_IMPL macro.
 */
#define CAT(...) CAT_DISPATCH(__VA_ARGS__)(__VA_ARGS__)
