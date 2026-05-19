#include "instruction_lowering.hpp"

#include "vm/core/fast/program/instructions/relocatable.hpp"

using namespace vm::loader::compiler;

std::vector<vm::fast::reloc::Instruction> vm::loader::compiler::fast::lowerInstructions(
	const vm::code::ValidProgram& high_program, const detail::FunctionStackContext& ctx
) {
	return { vm::fast::reloc::maker::mov_p64_imm(0, 42), vm::fast::reloc::maker::exit() };
}
