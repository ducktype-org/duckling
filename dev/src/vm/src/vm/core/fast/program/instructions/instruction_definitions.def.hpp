// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file instruction_definitions.def.hpp
 * @brief This file contains the definitions of all fast instructions.
 * Note that unlike in other places, here we are not appending argument names to the instruction
 * name, as this is performed automatically.
 * @author Mateusz Kołpa
 */

#include "argument_definitions.def.hpp"
#include "instr_macro_helper.def.hpp"

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

DEF_INSTR(init, (PlaceAny, dst), (Immediate, size))
DEF_INSTR(mov, (Place64, dst), (Immediate, imm))
DEF_INSTR(mov, (Place64, dst), (Place64, src))
DEF_INSTR(add, (Place64, dst), (Place64, src))
DEF_INSTR(add, (Place64, dst), (Immediate, imm))
DEF_INSTR(sub, (Place64, dst), (Place64, src))
DEF_INSTR(sub, (Place64, dst), (Immediate, imm))
DEF_INSTR(mod, (Place64, dst), (Place64, src))
DEF_INSTR(mod, (Place64, dst), (Immediate, imm))
DEF_INSTR(output, (Place64, src))
DEF_INSTR(input, (Place64, dst))
DEF_INSTR(cmpEq, (Place64, a), (Place64, b))
DEF_INSTR(cmpEq, (Place64, a), (Immediate, b))
DEF_INSTR(cmpGt, (Place64, a), (Place64, b))
DEF_INSTR(cmpGt, (Place64, a), (Immediate, b))
DEF_INSTR(jmpIf, (JumpDestination, target))
DEF_INSTR(jmpIfNot, (JumpDestination, target))
DEF_INSTR(jmp, (JumpDestination, target))
DEF_INSTR(
	call,
	(Function, func),
	(Immediate, stack_diff /* How much the stack needs to be adjusted, which is
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
