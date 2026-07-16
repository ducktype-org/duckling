#pragma once

#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>

namespace vm::fast::exec {
	/**
	 * @brief Links relocatable functions into executable functions, resolving argument references
	 * ahead of time so this work is done once here rather than repeatedly at runtime.
	 *
	 * Only the instruction *arguments* are transformed; the instruction sequence itself (its count
	 * and order) is preserved one-to-one. Most arguments are copied verbatim (immediates,
	 * stack/global offsets, relative jump destinations), while Function IDs become pointers into
	 * the returned collection and Type IDs are resolved into the matching `program.types[...]`
	 * objects. The sequence must be preserved because jump destinations are relative instruction
	 * offsets fixed during lowering — inserting or removing instructions here would invalidate them.
	 *
	 * @note Function pointers in the produced instructions point into the *returned*
	 * ExecFunctionCollection, so it must be kept alive (and not moved) while those functions run.
	 *
	 * @param program The program providing types and function metadata.
	 * @param reloc_functions The relocatable functions to link.
	 * @return The executable functions, with all cross-references resolved.
	 */
	ExecFunctionCollection linkFunctions(
		const ProgramBase& program, const reloc::RelocFunctionCollection& reloc_functions
	);
}
