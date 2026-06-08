#pragma once

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

#include <array>

namespace vm::jit::helpers {

	template<size_t number_of_exts = 0>
	void trampoline(OPFUN_REF_ARGS) {
		std::array<vm::MicroInstruction, 1 + number_of_exts + 1> start_function;
		for (size_t i = 0; i <= number_of_exts; ++i) start_function[i] = *(instr + i);
		start_function.back() = vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0);
		// = {
		//	*instr,
		// vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0),
		//	vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0),
		//};
		instr += 1 + number_of_exts;

		return runInterpreter(start_function.data(), local_stack, frame, thread);
	}
}
