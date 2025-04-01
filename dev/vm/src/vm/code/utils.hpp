#pragma once

#include "instructions.hpp"

#include <vm/code/opcode_args.hpp>

namespace vm::code::utils {
	/**
	 * @brief Tests whether two opcode arguments are equal.
	 */
	bool areArgsEqual(const vm::opargs::OpCodeArg& arg0, const vm::opargs::OpCodeArg& arg1);

	/**
	 * @brief Tests whether two opcode instructions are equal.
	 */
	bool areInstrEqual(const Instruction& instr0, const Instruction& instr1);
}
