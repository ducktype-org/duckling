#pragma once

#include "../ids.hpp"  // IWYU pragma: keep

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>

#include <vm/core/fast/eval/dispatcher.hpp>
#include <vm/core/fast/program/type.hpp>

namespace vm::fast {
	struct FunctionInfo;
}

namespace vm::fast::exec {
	struct ExecFunction;
	struct Instruction;

	namespace arg {
		using Immediate       = u64;
		using Place8          = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16         = u64;
		using Place32         = u64;
		using Place64         = u64;
		using PlaceAny        = u64;
		using Function        = const vm::fast::exec::ExecFunction*;
		using JumpDestination = i64;  // Pointer to the next instruction.
		using Type            = const Type*;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#define MAKE_INSTR_STRUCTS
#define MAKE_INSTRUCTION_UNION

#ifdef USE_TAIL_CALLS
	#define ID_TYPE() ::vm::fast::DispatcherFunction
	#define MAKE_MAKERS_JUST_DEF  // @note: No implementation in the .hpp
#else
	#define MAKE_MAKERS_FULL
#endif

#include "instr_structures.hpp"
#undef ARG_NAMESPACE
#undef ID_TYPE
#undef MAKE_INSTR_STRUCTS
#undef MAKE_INSTRUCTION_UNION
#undef MAKE_MAKERS

	struct ExecFunction {
		std::vector<Instruction> data;
		const FunctionInfo*      info;
	};

	using ExecFunctionCollection = std::vector<ExecFunction>;
}
