#pragma once

/**
 * @file relocatable.hpp
 * @brief The relocatable instruction representation used while building fast-mode programs.
 *
 * Fast mode has two instruction structs with the same layout but different argument types: the
 * relocatable form here (`reloc::Instruction`) and the executable form in `executable.hpp`
 * (`exec::Instruction`). The relocatable form stores arguments as compilation-stable IDs (a
 * `Function` is a `FunctionID`, a `Type` is a `TypeID`), while the executable form stores the same
 * arguments as raw runtime pointers. The lowering pipeline emits relocatable instructions, and
 * `relocator.hpp` links them into executable instructions just before execution; keeping the two
 * separate lets us build position-independent code first and resolve it to pointers only once the
 * final function/type storage is known.
 */

#include "../ids.hpp"

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>

#include <vector>

namespace vm::fast::reloc {
	namespace arg {
		using Immediate       = u64;
		using Place8          = u64;  // Encodes both local stack offsets and global buffer offsets,
		using Place16         = u64;
		using Place32         = u64;
		using Place64         = u64;
		using PlaceAny        = u64;
		using Function        = FunctionID;
		using JumpDestination = i64;  // Relative instruction jump offset after label linking.
		using Type            = TypeID;

#define HANDLE_ARG_DEF(arg) VALIDATE_ARG_EXISTS(arg)
#include "argument_definitions.def.hpp"  // Validates all needed arguments are defined
#undef HANDLE_ARG_DEF
	}

#define ARG_NAMESPACE arg::
#define MAKE_INSTR_STRUCTS
#define MAKE_INSTRUCTION_UNION
#define MAKE_MAKERS_FULL
#include "instr_structures.def.hpp"
#undef ARG_NAMESPACE
#undef MAKE_INSTR_STRUCTS
#undef MAKE_INSTRUCTION_UNION
#undef MAKE_MAKERS_FULL

	struct RelocFunction final {
		std::vector<Instruction> data;
	};

	using RelocFunctionCollection = std::vector<RelocFunction>;
}
