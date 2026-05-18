#pragma once

#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/program.hpp>

namespace vm::fast::exec {
	/**
	 * Links the functions in the program with the relocated functions.
	 * @param program The program to link.
	 * @param reloc_functions The relocatable functions.
	 * @return The linked functions.
	 */
	ExecFunctionCollection linkFunctions(
		const ProgramBase& program, const reloc::RelocFunctionCollection& reloc_functions
	);
}
