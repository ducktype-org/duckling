/**
 * @file instruction_definitions.hpp
 * @brief This file contains the definitions of all fast instructions.
 * Note that unlike in other places, here we are not appending argument names to the instruction
 * name, as this is performed automatically.
 * @author Mateusz Kołpa
 */

#include "argument_definitions.hpp"
#include "instr_macro_helper.hpp"

#ifndef HANDLE_INSTR
	#define DEFAULT_HANDLE_INSTR
	#define HANDLE_INSTR(instr)
#endif

#ifndef HANDLE_INSTR_ARGS
	#define DEFAULT_HANDLE_INSTR_ARGS
	#define HANDLE_INSTR_ARGS(instr, ...) HANDLE_INSTR(instr)
#endif

#ifndef DEF_INSTR
	#define DEFAULT_DEF_INSTR
	/**
     * @brief DEF_INSTR by default transforms the instruction name by appending the argument types
     * to it, and then calls HANDLE_INSTR with the transformed name.
     */
	#define DEF_INSTR(name, ...) HANDLE_INSTR_ARGS(INSTR_NAME(name, __VA_ARGS__), __VA_ARGS__)
#endif

DEF_INSTR(init, (Immediate, size))
DEF_INSTR(deinit, (Immediate, size))
DEF_INSTR(mov, (Place64, dst), (Immediate, imm))
DEF_INSTR(mov, (Place64, dst), (Place64, src))
DEF_INSTR(add, (Place64, dst), (Place64, src))
DEF_INSTR(sub, (Place64, dst), (Place64, src))
DEF_INSTR(output, (Place64, src))
DEF_INSTR(input, (Place64, dst))
DEF_INSTR(cmpEq, (Place64, a), (Place64, b))
DEF_INSTR(jumpIf, (JumpDestination, target))
DEF_INSTR(
	call,
	(Function, func),
	(
		Immediate, stack_diff /* How much the stack needs to be adjusted, which is
                                 equal to size of ret+args */
	)
)
DEF_INSTR(ret, (Immediate, function_return_size))

DEF_INSTR(exit)

#ifdef DEFAULT_HANDLE_INSTR
	#undef DEFAULT_HANDLE_INSTR
	#undef HANDLE_INSTR
#endif

#ifdef DEFAULT_HANDLE_INSTR_ARGS
	#undef DEFAULT_HANDLE_INSTR_ARGS
	#undef HANDLE_INSTR_ARGS
#endif

#ifdef DEFAULT_DEF_INSTR
	#undef DEFAULT_DEF_INSTR
	#undef DEF_INSTR
#endif
