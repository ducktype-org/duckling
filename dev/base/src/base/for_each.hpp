#pragma once
// Heavily inspired (copied) by : https://www.scs.stanford.edu/~dm/blog/va-opt.html

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
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
#define FOR_EACH(macro, ...)                                    \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...)                         \
  macro(a1)                                                     \
  __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER
