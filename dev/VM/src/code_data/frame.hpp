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

#include <base/optional.hpp>
#include <core/process/memory/pointer.hpp>
#include <core/process/memory/block.hpp>

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
		 * @brief  Current instruction in the stack frame.
		 * It is only updated when the new function is called.
		 */
		const struct Fix8Instruction* instr = nullptr;
		;

		/**
		 * @brief Memory array where the local variables are stored.
		 */
		std::byte* local_stack = nullptr;

		Registers regs;
		FlagData  flags{};

		/**
		 * @brief Return value of the function call.
		 */
		u64 ret_val{};

		/**
		 * @brief Place where the arguments for the
		 * future function call are stored (called "next arg stack").
		 */
		std::byte* next_args = nullptr;
		;

		/**
		 * @brief Place where the arguments for the
		 * current function call are stored (called "arg stack").
		 */
		std::byte* args = nullptr;
		;

		/**
		 * @brief Stack of block IDs used by the function created with init_type
		 * and destroyed with deinit.
		 */
		std::vector<Ref<Block>> block_stack;

		/**
		 * @brief First free byte in the local stack.
		 * Used when new block is created on the local stack.
		 */
		u64 local_stack_head{};

		Frame(): regs{ .p64_reg_0 = 0, .pointer_reg_0 = Pointer::null() } {}
	};
}
