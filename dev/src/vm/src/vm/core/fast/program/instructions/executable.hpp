#pragma once

#include "../ids.hpp"  // IWYU pragma: keep

#include <base/comptime/type_traits.hpp>
#include <base/comptime/is_complete.hpp>
#include <base/types/ints.hpp>

namespace vm::fast::exec {
	struct ExecFunction;
	struct Type;
	struct Instruction;

	namespace arg {
		using Immediate       = u64;
		using Place8          = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16         = u64;
		using Place32         = u64;
		using Place64         = u64;
		using Function        = vm::fast::exec::ExecFunction*;
		using JumpDestination = const Instruction*;  // Pointer to the next instruction.
		using Type            = Type*;

		template<class T>
		concept Place = base::IsOneOf<T, Place8, Place16, Place32, Place64>;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#include "instr_structures.hpp"
#undef ARG_NAMESPACE
}
