#define USE_SWITCH_CASE 1
#undef USE_TAIL_CALLS

#include "link_time.hpp"

#include <vm/core/jit/jit_compiler.hpp>
#include <vm/core/jit/jit_helper.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

// For situations when [[assume(...)]] gets ignored and it can't be.
#define FORCE_ASSUME(...) \
	if (!(__VA_ARGS__)) CORE_UNREACHABLE()

#define CALL_STENCIL(name) GET_LINK_VARIABLE(name, CP_RETURN (*)(CP_ARGS), 64)(CP_PASS_ARGS)

#define JUMP_STENCIL(name) MUST_TAIL return CALL_STENCIL(name)

#define CONTINUE_STENCIL JUMP_STENCIL(continue_fn)

namespace vm::jit::cnp {
	DECLARE_LINK_VARIABLE(instr_ptr);
	DECLARE_LINK_VARIABLE(arg0);
	DECLARE_LINK_VARIABLE(arg1);
	DECLARE_LINK_VARIABLE(continue_fn);
	DECLARE_LINK_VARIABLE(exception_thrower);

	template<OpFun* InstructionImplementation, low::MicroOpcode OpCode>
	__always_inline CP_RETURN base_stencil(CP_ARGS) {
		const MicroInstruction* instr = GET_LINK_VARIABLE(instr_ptr, const MicroInstruction*, 64);
		FORCE_ASSUME(instr->arg0 == GET_LINK_VARIABLE(arg0, u64, 64));
		FORCE_ASSUME(instr->arg1 == GET_LINK_VARIABLE(arg1, u64, 64));
		try {
			InstructionImplementation(instr, local_stack, frame, thread);
		} catch (...) { CALL_STENCIL(exception_thrower); }

		if constexpr (low::isOpcodeReturning<OpCode>) {
			// Since stencils do not take pointers/references it has to store the changed values.
			// Stencils do not take pointers to reduce the cost, since
			// it would have to be dereferenced or patched in every single stencil.
			return OpFuns::save_execution_state(instr, local_stack, frame, thread);
		}
		CONTINUE_STENCIL;
	}

// for now only a single(ext-less) instruction
// jitable_interface.py depends on the exact fully-qualified name
#define HANDLE_MICRO_INSTR(opcode_name)                                                   \
	CP_RETURN stencil_##opcode_name(CP_ARGS) {                                            \
		return base_stencil<vm::OpFuns::op_##opcode_name, low::MicroOpcode::opcode_name>( \
			CP_PASS_ARGS                                                                  \
		);                                                                                \
	}
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

	DECLARE_LINK_VARIABLE(jmp_fn);

	// NOLINTNEXTLINE(readability-identifier-naming)
	CP_RETURN stencil_special_jump_if(CP_ARGS) {
		if (frame->flags.flag)
			JUMP_STENCIL(jmp_fn);
		else
			CONTINUE_STENCIL;
	}

	// NOLINTNEXTLINE(readability-identifier-naming)
	CP_RETURN stencil_special_jump_if_not(CP_ARGS) {
		if (not frame->flags.flag)
			JUMP_STENCIL(jmp_fn);
		else
			CONTINUE_STENCIL;
	}

	// NOLINTNEXTLINE(readability-identifier-naming)
	CP_RETURN stencil_special_jump(CP_ARGS) { JUMP_STENCIL(jmp_fn); }

	DECLARE_LINK_VARIABLE(call_fn);
	DECLARE_LINK_VARIABLE(call_opcode);

	// NOLINTNEXTLINE(readability-identifier-naming)
	CP_RETURN stencil_special_call_non_jitable(CP_ARGS) {
		const MicroInstruction* instr = GET_LINK_VARIABLE(instr_ptr, const MicroInstruction*, 64);
		FORCE_ASSUME(instr->nontc_opcode == GET_LINK_VARIABLE(call_opcode, u64, 64));
		FORCE_ASSUME(instr->arg0 == GET_LINK_VARIABLE(arg0, u64, 64));
		FORCE_ASSUME(instr->arg1 == GET_LINK_VARIABLE(arg1, u64, 64));
		std::invoke(
			GET_LINK_VARIABLE(call_fn, vm::DebugOpFun*, 64), instr, local_stack, frame, thread
		);
		CONTINUE_STENCIL;
	}
}
