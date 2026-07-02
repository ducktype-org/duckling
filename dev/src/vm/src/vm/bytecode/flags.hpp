#pragma once
#include "instructions.hpp"

#include <base/extend_cpp/flag.hpp>

#include <vm/bytecode/bytecode.hpp>

#define INSTRUCTION_FLAG_OPTIONS                                                       \
	IORead, IOWrite, GlobalRead, GlobalWrite, Call, CallExternal, Multithread,         \
		RequiresGIL,          /* Instruction REQUIRES GIL */                           \
		ReleaseGIL,           /* Instruction MIGHT release GIL (takes a long time) */  \
		ControlFlowModifying, /* Modifies control flow, e.g. jmp, branch, call, ret */ \
		MayBlock

MAKE_FLAG_TYPE(vm::code, InstructionFlagOptions, InstructionFlag, INSTRUCTION_FLAG_OPTIONS)

// FunctionFlag deliberately carries the same options as InstructionFlag but is a distinct
// type: instruction flags describe a single instruction, function flags the aggregated
// effect of a whole function. Keeping them separate (rather than a `using` alias) forces an
// explicit InstructionFlag<->FunctionFlag translation and stops the two from being mixed up.
// In the feature some additional function-level flags may be added, e.g. "is pure" or
// "is deterministic", which would not make sense at the instruction level.
#define FUNCTION_FLAG_OPTIONS INSTRUCTION_FLAG_OPTIONS

MAKE_FLAG_TYPE(vm::code, FunctionFlagOptions, FunctionFlag, FUNCTION_FLAG_OPTIONS)

namespace vm::code {
	/**
	 * @brief Returns the flags describing observable effects of a builtin function.
	 * @TODO: #2716 propably move / remove this
	 */
	FunctionFlag getFlagsForBuiltinFunction(base::StrID name);

	/**
	 * @brief Returns the flags describing observable effects of an instruction.
	 *
	 * Place arguments are inspected against @p globals to decide if their access
	 * counts as a global read or a global write. Calls to builtin functions
	 * propagate the builtin's effect flags. Calls to external C functions are
	 * marked with CallExternal and treated as opaque (assumed to touch IO).
	 */
	InstructionFlag getFlagsForInstruction(
		Instruction                            instruction,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions
	);
}
