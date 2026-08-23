#pragma once

#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/loader/compiler/ivm_compiler.hpp>

namespace vm::loader::compiler::fast {
	/**
	 * @brief Lowers one validated high-level function body into fast-mode relocatable instructions.
	 *
	 * Each high-level instruction is translated into its fast-mode counterpart (variable names
	 * resolved to stack offsets, function names to IDs, etc.) and label references are rewritten
	 * into relative jump offsets.
	 *
	 * @param high_program Validated program, used to resolve functions/types by name.
	 * @param program_base Fast-mode program being built, providing type/function metadata.
	 * @param ctx Stack layout context for the function being lowered.
	 * @param func_info Fast-mode metadata of the function being lowered.
	 * @return The function body as relocatable instructions, ready to be linked by the relocator.
	 */
	std::vector<vm::fast::reloc::Instruction> lowerInstructions(
		const vm::code::ValidProgram&       high_program,
		const vm::fast::ProgramBase&        program_base,
		const detail::FunctionStackContext& ctx,
		const vm::fast::FunctionInfo&       func_info
	);
}
