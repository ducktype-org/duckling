#pragma once

#include "instruction_id.hpp"  // IWYU pragma: keep

#include <base/comptime/is_complete.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>

namespace vm::fast {
	class FunctionID;
	class TypeID;

}
namespace vm::fast::reloc {
	namespace arg {
		using Immediate    = u64;
		using Place8       = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16      = u64;
		using Place32      = u64;
		using Place64      = u64;
		using Function     = FunctionID;
		using JumpDistance = i64;  // Relative instruction jump offset after label linking.
		using Type         = TypeID;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#include "instr_common.hpp"
#undef ARG_NAMESPACE
}
