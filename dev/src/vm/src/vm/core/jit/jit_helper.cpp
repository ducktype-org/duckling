#include "jit_helper.hpp"

#include <base/except/exceptions.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

#include <array>

namespace vm::jit::helpers {
	void trampoline(OPFUN_REF_ARGS) {
		// Some instructions consume following ext_* slots as extra arguments. Of the instructions
		// that can be trampolined (the non-jittable ones), only virtual_call does so, reading its
		// local stack distance from a following ext_imm.
		const usize slot_count
			= vm::getInstructionOpcode(*instr) == vm::low::MicroOpcode::virtual_call_pptr_method
		        ? 2
		        : 1;
		CORE_ASSERT(
			slot_count == 1 || vm::getInstructionOpcode(instr[1]) == vm::low::MicroOpcode::ext_imm,
			"virtual_call must be followed by an ext_imm slot"
		);

		std::array<vm::MicroInstruction, 3> buffer{};
		for (usize i = 0; i < slot_count; ++i) buffer[i] = instr[i];
		// The exit bounds interpretation to this one instruction: a called function returns into
		// it, which stops the interpreter instead of continuing with whatever follows in bytecode.
		buffer[slot_count] = vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0);
		instr += slot_count;

		return runInterpreter(buffer.data(), local_stack, frame, thread);
	}
}
