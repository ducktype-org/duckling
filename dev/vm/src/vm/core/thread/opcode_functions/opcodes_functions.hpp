#pragma once

#include "../config.hpp"

#include <vm/core/thread/low_program/instruction.hpp>

#ifdef USE_TAIL_CALLS
	#define OPFUN_ARGS OPFUN_TC_ARGS
#else
	#define OPFUN_ARGS OPFUN_REF_ARGS
#endif

#ifdef USE_TAIL_CALLS
	#define RETURN_TYPE RETURN_TYPE_OPFUN_TC
#else
	#define RETURN_TYPE RETURN_TYPE_OPFUN_REF
#endif

namespace vm {
	using DebugOpFun = void(OPFUN_REF_ARGS);
	using OpFun      = void(OPFUN_ARGS);

	/**
	 * @brief A class that contains all opcode functions implementations.
	 * It is created to be a friend of the Thread and Process classes,
	 * so the instructions have access to the private members of these classes.
	 */
	class OpFuns final {
	public:
#define HANDLE_OPCODE(opcode) static OpFun op_##opcode;
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE

#define HANDLE_OPCODE(opcode) static DebugOpFun op_debug_##opcode;
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE

		// NOLINTBEGIN(readability-identifier-naming)
		// Opcodes utilities functions (named the similar way as all OpFuns)
		static OpFun handle_execution_break;
		static OpFun save_execution_state;
		// NOLINTEND(readability-identifier-naming)

		/**
		 * @brief A mapping between opcode ids and function pointers.
		 *
		 * @warning Ordering of elements must stay the same as in vm::OpcodeFix8
		 */
		static constexpr std::array<OpFun*, OP_CASES_COUNT> OPFUNS{
#define HANDLE_OPCODE(opcode) op_##opcode,
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE
		};

		/**
		 * @brief A mapping between opcode ids and debug function pointers.
		 */
		static constexpr std::array<DebugOpFun*, OP_CASES_COUNT> DEBUG_OPFUNS{
#define HANDLE_OPCODE(opcode) op_debug_##opcode,
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE
		};

		/**
		 * @brief Get the Opcode from the OpFun pointer.
		 */
		static u16 getOpcodeFromOpFun(OpFun* fun) {
			for (u16 i = 0; i < OP_CASES_COUNT; i++)
				if (OPFUNS.at(i) == fun) return i;
			CORE_UNREACHABLE();
		}
	};
}  // namespace vm
