/**
 * @file arguments.hpp
 * Include this file in order to statically verify all arguments types are defined.
 */

/**
 * @brief Validates that the given argument type exists and is complete.
 * This is a helper macro that can be used in HANDLE_ARG_DEF to validate that all argument types
 * used in instruction definitions are defined and complete.
 * @note To use this you need to include <base/comptime/is_complete.hpp> and define HANDLE_ARG_DEF
 * before including this file.
 */
#define VALIDATE_ARG_EXISTS(arg) \
	static_assert(IS_COMPLETE_V<arg>, "Type " #arg " must exist and be complete");

#ifndef HANDLE_ARG_DEF
	#define DEFAULT_HANDLE_ARG
	#define HANDLE_ARG_DEF(arg)
#endif

#ifndef DEF_ARG
	#define DEFAULT_DEF_ARG
	#define DEF_ARG(arg, ...) HANDLE_ARG_DEF(arg)
#endif

DEF_ARG(Immediate)
#define COMPARE_Immediate(x) x
#define INFO_Immediate()       (Immediate, imm)
DEF_ARG(Place8)
#define COMPARE_Place8(x) x
#define INFO_Place8()     (Place8, p8)
DEF_ARG(Place16)
#define COMPARE_Place16(x) x
#define INFO_Place16()     (Place16, p16)
DEF_ARG(Place32)
#define COMPARE_Place32(x) x
#define INFO_Place32()     (Place32, p32)
DEF_ARG(Place64)
#define COMPARE_Place64(x) x
#define INFO_Place64()     (Place64, p64)
DEF_ARG(Function)
#define COMPARE_Function(x) x
#define INFO_Function()     (Function, func)
DEF_ARG(JumpDistance)
#define COMPARE_JumpDistance(x) x
#define INFO_JumpDistance()     (JumpDistance, dist)

#ifdef DEFAULT_HANDLE_ARG
	#undef DEFAULT_HANDLE_ARG
	#undef HANDLE_ARG_DEF
#endif

#ifdef DEFAULT_DEF_ARG
	#undef DEFAULT_DEF_ARG
	#undef DEF_ARG
#endif

#undef VALIDATE_ARG_EXISTS
