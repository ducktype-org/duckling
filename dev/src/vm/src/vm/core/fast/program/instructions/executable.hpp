#pragma once

#include "instruction_id.hpp"  // IWYU pragma: keep

#include <base/comptime/is_complete.hpp>
#include <base/types/ints.hpp>

namespace vm::fast::exec {
	struct Function;
	struct Type;

	namespace arg {
		using Immediate    = u64;
		using Place8       = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16      = u64;
		using Place32      = u64;
		using Place64      = u64;
		using Function     = vm::fast::exec::Function*;
		using JumpDistance = i64;  // Relative instruction jump offset after label linking.

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#include "instr_common.hpp"
#undef ARG_NAMESPACE
}
