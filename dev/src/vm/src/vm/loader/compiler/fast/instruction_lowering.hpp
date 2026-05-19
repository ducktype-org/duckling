#pragma once

#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/loader/compiler/compiler.hpp>

namespace vm::loader::compiler::fast {
	std::vector<vm::fast::reloc::Instruction> lowerInstructions(
		const vm::code::ValidProgram& high_program, const detail::FunctionStackContext& ctx
	);
}
