#include <base/comptime/macro_magic.hpp>
#include <base/preproc/for_each.hpp>

#define FIRST(a, ...)     a
#define SECOND(_, b, ...) b

#define ARG_NAME_SHORT(arg)             SECOND CAT(INFO_, FIRST arg)()
#define COMMA_ARG_NAME_SHORT_FLOOR(arg) , DEFER(CAT)(_, ARG_NAME_SHORT(arg))

#define EXPAND_ARGS_AS_CAT_ARGS(...) FOR_EACH(COMMA_ARG_NAME_SHORT_FLOOR, __VA_ARGS__)

#define INSTR_NAME(instr, ...) EVAL(DEFER(CAT)(instr EXPAND_ARGS_AS_CAT_ARGS(__VA_ARGS__)))
