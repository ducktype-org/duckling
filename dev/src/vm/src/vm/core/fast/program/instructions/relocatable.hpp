#pragma once

#include "../ids.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/comptime/is_complete.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>

namespace vm::fast::reloc {
	namespace arg {
		using Immediate       = u64;
		using Place8          = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16         = u64;
		using Place32         = u64;
		using Place64         = u64;
		using Function        = FunctionID;
		using JumpDestination = i64;  // Relative instruction jump offset after label linking.
		using Type            = TypeID;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#include "instr_structures.hpp"
#undef ARG_NAMESPACE
}
