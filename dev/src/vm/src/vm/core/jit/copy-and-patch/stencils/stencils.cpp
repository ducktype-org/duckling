#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALLS

#include "link_time.hpp"

#include <vm/core/jit/jit_compiler.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <iostream>

// For situations when [[assume(...)]] gets ignored and it can't be.
#define FORCE_ASSUME(...) \
	if (!(__VA_ARGS__)) CORE_UNREACHABLE()

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

		FORCE_ASSUME(instr->nontc_opcode == std::to_underlying(OpCode));
		FORCE_ASSUME(instr->arg0 == GET_LINK_VARIABLE(arg0, u64, 64));
		FORCE_ASSUME(instr->arg1 == GET_LINK_VARIABLE(arg1, u64, 64));
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
		// Since stencils do not take pointers/references it has to store the changed values.
		// Stencils do not take pointers to reduce the cost, since
		// it would have to be dereferenced or patched in every single stencil.
		thread.runtime_data.frame_stack_current = frame;
		frame->local_stack                      = local_stack;
		frame->instr                            = instr;
		return;
	}
}
