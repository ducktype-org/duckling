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

	// VLA: data:
	struct VLADataReference {
		std::byte* local_stack;
	};

	/**
	 * @brief This is temporary structure that is used to
	 * easily pass parameters to function, without proper „argument stack”
	 *
	 * In the future special calling conventions can be added to quickly call
	 * functions with common signatures
	 */
	struct StandardFunctionArgs {
		u64     p64_arg;
		Pointer pointer_arg;
	};

	struct Frame {
		// Internal data:
    base::borrow_ptr<Frame> previous;

		bool  continue_execution;
		usize instruction_pointer;

		// Register like data:
		Registers            regs;
		FlagData             flags;
		u64                  ret_val;
		StandardFunctionArgs next_args;

		// Corresponds to current head of std::byte local_stack[] which is passed
		// to op functions directly for faster access
		u64 local_stack_head;

		// @TODO: static code analysis could be done to determine the smallest
		// possible stack size for block_ids of variables
		BlockId* block_id_stack;
		u64      block_id_stack_head;

		StandardFunctionArgs args;

		// Local stack:
		VLADataReference vla_data_reference;

		// clang-tidy complains about this, becasue having a reference
		// memeber disables copy-assignment. This doesn't seem to be relevant to us
		// though we would need to explicilt remove these ctors or use std::reference_wrapper
		// NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
		class Executor& executor;
		// NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
	};
}
