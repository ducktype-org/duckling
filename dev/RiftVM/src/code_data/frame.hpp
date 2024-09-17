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
		// Program control flow:
		const struct Fix8Instruction* instr;
		std::byte*                    local_stack;

		// Register like data:
		Registers            regs;
		FlagData             flags;
		u64                  ret_val;
		StandardFunctionArgs next_args;
		StandardFunctionArgs args;

		std::vector<BlockID> block_id_stack;
		u64                  local_stack_head;
	};
}
