#include "jit_compiler.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/vmthread.hpp>

#include <array>
#include <iostream>

extern "C" void externalTrampoline(void* _instr, void* _stack, void* _frame, void* _thread) {
	const vm::MicroInstruction** instr  = reinterpret_cast<const vm::MicroInstruction**>(_instr);
	std::byte**                  stack  = reinterpret_cast<std::byte**>(_stack);
	vm::Frame**                  frame  = reinterpret_cast<vm::Frame**>(_frame);
	vm::VMThread*                thread = reinterpret_cast<vm::VMThread*>(_thread);

	std::array<vm::MicroInstruction, 2> start_function = {
		**instr,
		vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0),
	};
	++(*instr);

	return runInterpreter(start_function.data(), *stack, *frame, *thread);
}
