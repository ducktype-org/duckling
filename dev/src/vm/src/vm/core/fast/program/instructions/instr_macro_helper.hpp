#include <base/preproc/cat.hpp>
#include <base/preproc/for_each.hpp>

#define ARG_NAME_SHORT(arg)             FIRST CAT(INFO_, FIRST arg)()
#define COMMA_ARG_NAME_SHORT_FLOOR(arg) , DEFER(CAT)(_, ARG_NAME_SHORT(arg))

#define EXPAND_ARGS_AS_CAT_ARGS(...) FOR_EACH(COMMA_ARG_NAME_SHORT_FLOOR, __VA_ARGS__)

#define INSTR_NAME(instr, ...) EVAL(DEFER(CAT)(instr EXPAND_ARGS_AS_CAT_ARGS(__VA_ARGS__)))
