#pragma once

#define EXPAND(...)       __VA_ARGS__
#define APPLY(macro, ...) EXPAND(macro(__VA_ARGS__))

#define FOR_EACH_1(macro, x)       APPLY(macro, x)
#define FOR_EACH_2(macro, x, ...)  APPLY(macro, x) FOR_EACH_1(macro, __VA_ARGS__)
#define FOR_EACH_3(macro, x, ...)  APPLY(macro, x) FOR_EACH_2(macro, __VA_ARGS__)
#define FOR_EACH_4(macro, x, ...)  APPLY(macro, x) FOR_EACH_3(macro, __VA_ARGS__)
#define FOR_EACH_5(macro, x, ...)  APPLY(macro, x) FOR_EACH_4(macro, __VA_ARGS__)
#define FOR_EACH_6(macro, x, ...)  APPLY(macro, x) FOR_EACH_5(macro, __VA_ARGS__)
#define FOR_EACH_7(macro, x, ...)  APPLY(macro, x) FOR_EACH_6(macro, __VA_ARGS__)
#define FOR_EACH_8(macro, x, ...)  APPLY(macro, x) FOR_EACH_7(macro, __VA_ARGS__)
#define FOR_EACH_9(macro, x, ...)  APPLY(macro, x) FOR_EACH_8(macro, __VA_ARGS__)
#define FOR_EACH_10(macro, x, ...) APPLY(macro, x) FOR_EACH_9(macro, __VA_ARGS__)
#define FOR_EACH_11(macro, x, ...) APPLY(macro, x) FOR_EACH_10(macro, __VA_ARGS__)
#define FOR_EACH_12(macro, x, ...) APPLY(macro, x) FOR_EACH_11(macro, __VA_ARGS__)
#define FOR_EACH_13(macro, x, ...) APPLY(macro, x) FOR_EACH_12(macro, __VA_ARGS__)
#define FOR_EACH_14(macro, x, ...) APPLY(macro, x) FOR_EACH_13(macro, __VA_ARGS__)
#define FOR_EACH_15(macro, x, ...) APPLY(macro, x) FOR_EACH_14(macro, __VA_ARGS__)
#define FOR_EACH_16(macro, x, ...) APPLY(macro, x) FOR_EACH_15(macro, __VA_ARGS__)
#define FOR_EACH_17(macro, x, ...) APPLY(macro, x) FOR_EACH_16(macro, __VA_ARGS__)
#define FOR_EACH_18(macro, x, ...) APPLY(macro, x) FOR_EACH_17(macro, __VA_ARGS__)
#define FOR_EACH_19(macro, x, ...) APPLY(macro, x) FOR_EACH_18(macro, __VA_ARGS__)
#define FOR_EACH_20(macro, x, ...) APPLY(macro, x) FOR_EACH_19(macro, __VA_ARGS__)
#define FOR_EACH_21(macro, x, ...) APPLY(macro, x) FOR_EACH_20(macro, __VA_ARGS__)
#define FOR_EACH_22(macro, x, ...) APPLY(macro, x) FOR_EACH_21(macro, __VA_ARGS__)
#define FOR_EACH_23(macro, x, ...) APPLY(macro, x) FOR_EACH_22(macro, __VA_ARGS__)
#define FOR_EACH_24(macro, x, ...) APPLY(macro, x) FOR_EACH_23(macro, __VA_ARGS__)
#define FOR_EACH_25(macro, x, ...) APPLY(macro, x) FOR_EACH_24(macro, __VA_ARGS__)
#define FOR_EACH_26(macro, x, ...) APPLY(macro, x) FOR_EACH_25(macro, __VA_ARGS__)
#define FOR_EACH_27(macro, x, ...) APPLY(macro, x) FOR_EACH_26(macro, __VA_ARGS__)
#define FOR_EACH_28(macro, x, ...) APPLY(macro, x) FOR_EACH_27(macro, __VA_ARGS__)
#define FOR_EACH_29(macro, x, ...) APPLY(macro, x) FOR_EACH_28(macro, __VA_ARGS__)
#define FOR_EACH_30(macro, x, ...) APPLY(macro, x) FOR_EACH_29(macro, __VA_ARGS__)

#define GET_FOR_EACH_MACRO( \
	_1,                     \
	_2,                     \
	_3,                     \
	_4,                     \
	_5,                     \
	_6,                     \
	_7,                     \
	_8,                     \
	_9,                     \
	_10,                    \
	_11,                    \
	_12,                    \
	_13,                    \
	_14,                    \
	_15,                    \
	_16,                    \
	_17,                    \
	_18,                    \
	_19,                    \
	_20,                    \
	_21,                    \
	_22,                    \
	_23,                    \
	_24,                    \
	_25,                    \
	_26,                    \
	_27,                    \
	_28,                    \
	_29,                    \
	_30,                    \
	NAME,                   \
	...                     \
)                           \
	NAME

/**
 * @brief Macro that applies `to_apply` on all arguments.
 * @note Number of arguments is currently limited to 30, but it can be easily increased.
 *
 * @example:
 * ```cpp
 * #define F(x) struct S##x {};
 # FOR_EACH(F, 1, 2, 3, 4)
 * ```
 * expands to:
 * ```cpp
 * struct S1 {};
 * struct S2 {};
 * struct S3 {};
 * struct S4 {};
 * ```
 */
#define FOR_EACH(to_apply, ...)                                                                                                                                                                                                                                                                                                                                                                                                       \
	EXPAND(                                                                                                                                                                                                                                                                                                                                                                                                                           \
		GET_FOR_EACH_MACRO(__VA_ARGS__, FOR_EACH_30, FOR_EACH_29, FOR_EACH_28, FOR_EACH_27, FOR_EACH_26, FOR_EACH_25, FOR_EACH_24, FOR_EACH_23, FOR_EACH_22, FOR_EACH_21, FOR_EACH_20, FOR_EACH_19, FOR_EACH_18, FOR_EACH_17, FOR_EACH_16, FOR_EACH_15, FOR_EACH_14, FOR_EACH_13, FOR_EACH_12, FOR_EACH_11, FOR_EACH_10, FOR_EACH_9, FOR_EACH_8, FOR_EACH_7, FOR_EACH_6, FOR_EACH_5, FOR_EACH_4, FOR_EACH_3, FOR_EACH_2, FOR_EACH_1)( \
			to_apply, __VA_ARGS__                                                                                                                                                                                                                                                                                                                                                                                                     \
		)                                                                                                                                                                                                                                                                                                                                                                                                                             \
	)
