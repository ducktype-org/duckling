/**
 * @file frame.hpp
 * @brief Defines the stack frame structure used by the Executor module.
 *
 * The stack frame is used to store the state of the program during its execution.
 * More info in the paper: ["Nowoczesne metody optymalizacji..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/pondvm-opt-pl.pdf)
 */
#pragma once

#include <base/ints.hpp>

#include <base/optional.hpp>
#include <memory_data/pointer.hpp>

namespace vm {

	// Non-VLA data:
	struct Registers {
		u64     p64_reg_0;
		Pointer pointer_reg_0;
	};

	struct FlagData {
		bool flag;
	};

	/**
	 * @brief Stack frame structure used by the Executor module.
	 * 
	 * It stores the state of the one function call during the program execution.
	 */
	struct Frame {
		/**
		 * Current instruction in the stack frame.
		 * It is only updated when the new function is called.
		 */
		const struct Fix8Instruction* instr;
		std::byte*                    local_stack;

		// Register like data:
		Registers  regs;
		FlagData   flags;
		u64        ret_val;
		std::byte* next_args;
		std::byte* args;
		
		/**
		 * Stack of block IDs used by the function.
		 */
		std::vector<BlockID> block_id_stack;
		/**
		 * First free byte in the local stack.
		 * Used when new block is created on the local stack.
		 */
		u64                  local_stack_head;
	};
}
