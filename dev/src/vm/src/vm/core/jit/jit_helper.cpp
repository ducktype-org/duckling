#include "jit_helper.hpp"

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

#include <array>

namespace vm::jit::helpers {
	void trampoline(OPFUN_ARGS) {
		std::array<vm::MicroInstruction, 2> start_function = {
			*instr,
			vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0),
		};
		++instr;

		return runInterpreter(start_function.data(), local_stack, frame, thread);
	}
}
