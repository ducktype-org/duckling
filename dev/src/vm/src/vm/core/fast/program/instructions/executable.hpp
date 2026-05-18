#pragma once

#include "../ids.hpp"  // IWYU pragma: keep

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>

#include <vm/core/config.hpp>
#include <vm/core/fast/program/type.hpp>

namespace vm::fast::exec {
	struct ExecFunction;
	struct Instruction;

	namespace arg {
		using Immediate       = u64;
		using Place8          = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16         = u64;
		using Place32         = u64;
		using Place64         = u64;
		using Function        = const vm::fast::exec::ExecFunction*;
		using JumpDestination = const Instruction*;  // Pointer to the next instruction.
		using Type            = const Type*;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#ifdef USE_TAIL_CALLS
	#error tails calls are not yet supported
#endif

#define ARG_NAMESPACE arg::
#include "instr_structures.hpp"
#undef ARG_NAMESPACE

	struct ExecFunction {
		std::vector<Instruction> data;
	};

	using ExecFunctionCollection = std::vector<ExecFunction>;
}
