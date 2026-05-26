#pragma once

#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/loader/compiler/ivm_compiler.hpp>

namespace vm::loader::compiler::fast {
	std::vector<vm::fast::reloc::Instruction> lowerInstructions(
		const vm::code::ValidProgram&       high_program,
		const vm::fast::ProgramBase&        program_base,
		const detail::FunctionStackContext& ctx,
		const vm::fast::FunctionInfo&       func_info
	);
}
