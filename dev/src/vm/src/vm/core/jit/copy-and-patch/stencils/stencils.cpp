#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALLS

#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

namespace vm::jit::cnp {

// for now only a single(ext-less) instruction
// opcodes_interface.py depends on the exact fully-qualified name
#define HANDLE_MICRO_INSTR(opcode_name)                                               \
	void stencil_##opcode_name(                                                       \
		MicroInstruction instr, byte* local_stack, Frame* frame, SafeVMThread& thread \
	) {                                                                               \
		CORE_ASSERT(                                                                  \
			getInstructionOpcode(instr) == low::MicroOpcode::opcode_name,             \
			"Expected a different opcode"                                             \
		);                                                                            \
		const MicroInstruction* instr_ptr = &instr;                                   \
		vm::OpFuns::op_##opcode_name(instr_ptr, local_stack, frame, thread);          \
		CORE_ASSERT(instr_ptr == &instr + 1, "An unexpected jumping opcode");         \
	}
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

}
