#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALL

#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>

namespace vm {

// for now only a single(ext-less) instruction
#define HANDLE_MICRO_INSTR(opcode_name)                                                       \
	void wrapper_##opcode_name(                                                               \
		MicroInstruction instr, std::byte* local_stack, Frame* frame, VMThread& thread        \
	) {                                                                                       \
		const MicroInstruction* instr_ptr = &instr;                                           \
		vm::OpFuns::op_##opcode_name(instr_ptr, local_stack, frame, thread);                  \
		CORE_ASSERT(instr_ptr == &instr + 1, "An unexpected jumping opcode");                 \
	}
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

}
