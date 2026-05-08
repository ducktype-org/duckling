#pragma once

#include <base/types/ints.hpp>
#include <base/comptime/is_complete.hpp>

#include "../instruction_id.hpp"

namespace vm::fast::exec {

	using Immediate    = u64;
	using Place8       = u64;  // Encodes both local stack offsets and global buffer offsets,
	using Place16      = u64;
	using Place32      = u64;
	using Place64      = u64;
	using Function     = void*;
	using JumpDistance = i64;  // Relative instruction jump offset after label linking.

    #define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
    #include "../argument_definitions.hpp"    // Validates all needed arguments are defined
    #undef HANDLE_ARG_DEF

	#include "../instr_common.hpp"
}
