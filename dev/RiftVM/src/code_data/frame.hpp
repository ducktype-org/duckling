#pragma once

#include <base/ints.hpp>
#include <vector>
#include <span>

#include <memory_data/pointer.hpp>
#include "code.hpp"
#include <base/optional.hpp>

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
		Frame(Frame&& frame) {
			previous            = frame.previous;
			bc                  = frame.bc;
			continue_execution  = frame.continue_execution;
			instruction_pointer = frame.instruction_pointer;

			// Register like data:
			regs      = frame.regs;
			flags     = frame.flags;
			ret_val   = frame.ret_val;
			next_args = frame.next_args;
		}

		// Internal data:
		base::Optional<Frame&> previous;
		// const FuncData& function;
		std::span<const Fix8Instruction>
			bc;  // this is duplication of function.bc, but allows for faster access

		bool  continue_execution;
		usize instruction_pointer;

		// Register like data:
		Registers            regs;
		FlagData             flags;
		u64                  ret_val;
		StandardFunctionArgs next_args;

		// Local stack:
		VLADataReference vla_data_reference;
	};
}
