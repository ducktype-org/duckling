
#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALL

#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>

// for now only a single(ext-less) instruction
template<low::MicroOpcode nontc_opcode>
void wrapper(MicroInstruction instr, std::byte* stack, Frame* frame, Thread* thread) {
	CORE_ASSERT(instr.nontc_opcode == non_tc_opcode, "Expected a different opcode");

	MicroInstruction* instr_ptr = &instr;
	switch (nontc_opcode) {
#define HANDLE_MICRO_INSTR(opcode_name)                                      \
	case low::MicroOpcode::opcode_name: {                                    \
		vm::OpFuns::op_##opcode_name(instr_ptr, local_stack, frame, thread); \
	}
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

	default:
		CORE_PANIC("Unknown opcode");
	}

	CORE_ASSERT(instr_ptr == &instr + 1, "An unexpected jumping opcode");
}

#define HANDLE_OPCODE(opcode_name) wrapper<low::MicroOpcode::opcode_name>;
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_OPCODE
