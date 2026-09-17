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

	/**
	 * @brief One local variable slot: what the variable is, where it lives, and its block once
	 * something refers to the variable through one.
	 */
	struct LocalSlot final {
		/// Never null on a live slot: every entry below `local_slot_stack_end` is written by
		/// `pushLocalSlot`, which always takes a real `TypeCRef`, and nobody reads past that end.
		/// A raw pointer rather than a `TypeCRef` only because the slot stack is default
		/// constructed whole.
		const Type* type = nullptr;
		/// Absolute, so that a slot shared with the caller reads alike from both frames.
		byte* data = nullptr;
		/// Null until an instruction first needs a block, which is then built out of the two
		/// fields above.
		Block* block = nullptr;
	};

	struct FlagData final {
		// CRITICAL: Field flag must be defined first due to rules of field accessing in LLVM (used
		// for JIT purposes)
		bool flag;
	};

	/**
	 * @brief Stack frame structure used by the Executor module.
	 *
	 * It stores the state of the one function call during the program execution.
	 */
	struct Frame final {
		// CRITICAL: Field flags must be defined first due to rules of field accessing in LLVM (used
		// for JIT purposes)
		FlagData flags{};

		/**
		 * @brief Where the frame resumes from.
		 *
		 * While a call is in progress this is the caller's return address, set by
		 * `performFunctionCall` and read back by `ret`. While the thread is paused it is the
		 * instruction the frame stopped on, which is what `save_execution_state`,
		 * `executeOneStep` and `getCurrentOpcode` write and read.
		 */
		const struct MicroInstruction* instr = nullptr;

		/**
		 * @brief Memory array where the local variables are stored.
		 */
		byte* local_stack = nullptr;

		/**
		 * @brief Base of the stack of local variable slots, indexed by slot index. A slot is
		 * pushed by init and dropped by deinit.
		 */
		LocalSlot* local_slot_stack_base = nullptr;

		/**
		 * @brief The pointer to the first free position on the slot stack.
		 */
		LocalSlot* local_slot_stack_end = nullptr;

		/**
		 * @brief Function linked to the frame.
		 * If frame doesn't change, but a function does (e.g. tailcall), this pointer should be
		 * updated accordingly, so that it's always valid.
		 */
		MCRef<low::LowFuncData> current_function;

		void resetFrameData() { *this = Frame(); }
	};
}
