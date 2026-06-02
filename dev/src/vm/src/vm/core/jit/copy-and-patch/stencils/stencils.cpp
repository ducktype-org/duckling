#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALLS

#include "link_time.hpp"

#include <vm/core/jit/jit_compiler.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <iostream>

namespace vm::jit::cnp {
	DECLARE_LINK_VARIABLE(continue_fn);
	DECLARE_LINK_VARIABLE(arg0);
	DECLARE_LINK_VARIABLE(arg1);

	template<OpFun* InstructionImplementation, low::MicroOpcode OpCode>
	__always_inline void base_stencil(CP_ARGS) {
		CORE_ASSERT(
			OpCode == low::MicroOpcode::exit || getInstructionOpcode(*instr) == OpCode,
			"Expected a different opcode",
		);

		if (instr->nontc_opcode != std::to_underlying(OpCode)) CORE_UNREACHABLE();
		if (instr->arg0 != GET_LINK_VARIABLE(arg0, u64, 64)) CORE_UNREACHABLE();
		if (instr->arg1 != GET_LINK_VARIABLE(arg1, u64, 64)) CORE_UNREACHABLE();
		InstructionImplementation(instr, local_stack, frame, thread);


		auto continue_fn = GET_LINK_VARIABLE(
			continue_fn, void (*)(const MicroInstruction*, byte*, Frame*, SafeVMThread&), 64
		);
		return (*continue_fn)(instr, local_stack, frame, thread);
	}

// for now only a single(ext-less) instruction
// jitable_interface.py depends on the exact fully-qualified name
#define HANDLE_MICRO_INSTR(opcode_name)                                                   \
	extern "C" void stencil_##opcode_name(CP_ARGS) {                                      \
		return base_stencil<vm::OpFuns::op_##opcode_name, low::MicroOpcode::opcode_name>( \
			instr, local_stack, frame, thread                                             \
		);                                                                                \
	}

#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

	extern "C" void stencil_special_return(CP_ARGS) {
		thread.runtime_data.frame_stack_current = frame;
		frame->local_stack                      = local_stack;
		frame->instr                            = instr;
		return;
	}
}
