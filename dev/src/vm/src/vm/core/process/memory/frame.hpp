/**
 * @file frame.hpp
 * @brief Defines the stack frame structure used by the Executor module.
 *
 * The stack frame is used to store the state of the program during its execution.
 * More info in the paper: ["Nowoczesne metody
 * optymalizacji..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/pondvm-opt-pl.pdf)
 */
#pragma once

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>

#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/memory/pointer.hpp>

#include <cstddef>

namespace vm {

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
		 * @brief  Current instruction in the stack frame.
		 * It is only updated when the new function is called.
		 */
		const struct MicroInstruction* instr = nullptr;

		/**
		 * @brief Memory array where the local variables are stored.
		 */
		std::byte* local_stack = nullptr;

		FlagData flags{};

		/**
		 * @brief Size of arguments that were passed to a function called by this one.
		 * Needed to restore the local_stack_head when returning from a function.
		 */
		u64 called_func_arg_size = 0;

		/**
		 * @brief Size of the return value of the function called by this one.
		 * Needed to restore the local_stack_head when returning from a function.
		 */
		u64 called_func_ret_size = 0;

		/**
		 * @brief Stack of block IDs used by the function created with init_type
		 * and destroyed with deinit.
		 */
		std::vector<Ref<Block>> block_stack;

		/**
		 * @brief Mapping from stack offset to ID of block
		 * responsible for data on that offset.
		 *
		 * Used when creating pointers to local variables.
		 */
		base::HashMap<u64, u64> local_offset_to_block_idx;

		/**
		 * @brief Mapping from ID of block to the offset on the local stack.
		 *
		 * Used when calling and returning from the function to populate the
		 * local_offset_to_block_idx of the called function (to make is possible
		 * to create a pointer to a passed argument).
		 */
		base::HashMap<u64, u64> block_idx_to_local_offset;

		/**
		 * @brief First free byte in the local stack.
		 * Used when new block is created on the local stack.
		 */
		u64 local_stack_head = 0;

		void resetFrameData() { *this = Frame(); }
	};
}
