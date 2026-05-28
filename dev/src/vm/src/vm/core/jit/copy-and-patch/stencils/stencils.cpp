#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALLS

#include "link_time.hpp"

#include <vm/core/jit/jit_compiler.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

namespace vm::jit::cnp {

// for now only a single(ext-less) instruction
// jitable_interface.py depends on the exact fully-qualified name
#define HANDLE_MICRO_INSTR(opcode_name)                                                      \
	extern "C" void stencil_##opcode_name(                                                              \
		const MicroInstruction* instr, byte* local_stack, Frame* frame, SafeVMThread& thread \
	) {                                                                                      \
		CORE_ASSERT(                                                                         \
			getInstructionOpcode(*instr) == low::MicroOpcode::opcode_name,                   \
			"Expected a different opcode"                                                    \
		);                                                                                   \
		MicroInstruction        my_instr  = *instr;                                          \
		const MicroInstruction* instr_ptr = &my_instr;                                       \
                                                                                             \
		vm::OpFuns::op_##opcode_name(instr_ptr, local_stack, frame, thread);                 \
                                                                                             \
		CORE_ASSERT(instr_ptr == &my_instr + 1, "An unexpected jumping opcode");             \
                                                                                             \
		auto continue_fn = LINK_VALUE(                                                       \
			continue_fn, void (*)(const MicroInstruction*, byte*, Frame*, SafeVMThread&), 64 \
		);                                                                                   \
		return (*continue_fn)(instr, local_stack, frame, thread);                            \
	}
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

}
