/**
 * @file frame.hpp
 * @brief Defines the stack frame structure used by the Executor module.
 *
 * The stack frame is used to store the state of the program during its execution.
 * More info in the paper: ["Nowoczesne metody
 * optymalizacji..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/pondvm-opt-pl.pdf)
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/block.hpp>
#include <vm/core/safe/memory/pointer.hpp>

#include <cstddef>

namespace vm {
	namespace low {
		struct LowFuncData;
	}

	class Type;

	/**
	 * @brief Type and location of the variable living in one local variable slot, which is what
	 * a block is made out of when something first refers to the variable through one.
	 */
	struct LocalSlot {
		const Type* type = nullptr;
		/// Absolute, so that a slot shared with the caller reads alike from both frames.
		byte* data = nullptr;
	};

	struct FlagData {
		// CRITICAL: Field flag must be defined first due to rules of field accessing in LLVM (used
		// for JIT purposes)
		bool flag;
	};

	/**
	 * @brief Stack frame structure used by the Executor module.
	 *
	 * It stores the state of the one function call during the program execution.
	 */
	struct Frame {
		// CRITICAL: Field flags must be defined first due to rules of field accessing in LLVM (used
		// for JIT purposes)
		FlagData flags{};

		/**
		 * @brief Where to resume: the return address while a call is in progress, and the
		 * instruction the frame stopped on while the thread is paused.
		 */
		const struct MicroInstruction* return_address = nullptr;

		/**
		 * @brief Memory array where the local variables are stored.
		 */
		byte* local_stack = nullptr;

		/**
		 * @brief Base of the stack of block IDs used by the function created with init_type
		 * and destroyed with deinit.
		 */
		Block** local_block_ref_stack_base = nullptr;

		/**
		 * @brief The pointer to the first free position on the block stack.
		 */
		Block** local_block_ref_stack_end = nullptr;

		/// Base of the stack of local variable slots, indexed by slot index.
		LocalSlot* local_slot_stack_base = nullptr;

		/**
		 * @brief Function linked to the frame.
		 * If frame doesn't change, but a function does (e.g. tailcall), this pointer should be
		 * updated accordingly, so that it's always valid.
		 */
		MCRef<low::LowFuncData> current_function;

		void resetFrameData() { *this = Frame(); }
	};
}
