// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#ifndef HANDLE_NONJITTABLE_INSTR
	#define HANDLE_NONJITTABLE_INSTR(instr)
#endif

HANDLE_NONJITTABLE_INSTR(jitFuncEntrypoint)
HANDLE_NONJITTABLE_INSTR(jitLoopEntrypoint)
HANDLE_NONJITTABLE_INSTR(call_func_off)
HANDLE_NONJITTABLE_INSTR(call_builtinfunc)
HANDLE_NONJITTABLE_INSTR(virtualCall_pptr_method)
HANDLE_NONJITTABLE_INSTR(call_ffifunc)
HANDLE_NONJITTABLE_INSTR(stepGil)
HANDLE_NONJITTABLE_INSTR(checkStrategy)
HANDLE_NONJITTABLE_INSTR(breakpoint)
