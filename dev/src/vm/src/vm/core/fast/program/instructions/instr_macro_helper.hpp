/**
 * @file instr_macro_helper.hpp
 * @brief Helper to build a mangled instruction name from its argument types.
 *
 * `INSTR_NAME(base, ArgType...)` expands to `base_<info0>_<info1>...`, where each suffix comes from
 * the `INFO_<ArgType>()` macro (e.g. `INFO_Place8()` -> `(p8)`). It therefore only works for
 * argument types that have a matching `INFO_<ArgType>` macro defined (see argument_definitions.hpp).
 */

#include <base/preproc/cat.hpp>
#include <base/preproc/for_each.hpp>

#define ARG_NAME_SHORT(arg)             FIRST CAT(INFO_, FIRST arg)()
#define COMMA_ARG_NAME_SHORT_FLOOR(arg) , DEFER(CAT)(_, ARG_NAME_SHORT(arg))

#define EXPAND_ARGS_AS_CAT_ARGS(...) FOR_EACH(COMMA_ARG_NAME_SHORT_FLOOR, __VA_ARGS__)

#define INSTR_NAME(instr, ...) EVAL(DEFER(CAT)(instr EXPAND_ARGS_AS_CAT_ARGS(__VA_ARGS__)))
