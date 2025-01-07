/**
 * @file instruction.hpp
 * @brief Defines the instruction structure (types) and the opcodes functions used by the Executor
 * and Parser modules.
 */
#pragma once

#include "frame.hpp"
#include <config.hpp>
#include <base/ints.hpp>
#include <array>

// #define USE_COMPACT_INSTRUCTION

#define OPFUN_ARGS                                                    \
	const Fix8Instruction *IF_NOT_TC(&) instr [[maybe_unused]],       \
		std::byte *        IF_NOT_TC(&) local_stack [[maybe_unused]], \
		Frame *IF_NOT_TC(&) frame [[maybe_unused]], VMThread &thread [[maybe_unused]]

#define RETURN_TYPE IF_NOT_TC([[gnu::always_inline]] inline) void

namespace {
	/**
	 * @brief Returns number of opcodes recognized by Executor in a compile-time.
	 * Used for `vm::OP_CASES_COUNT`.
	 *
	 * @return constexpr u16
	 */
	constexpr u16 countOpCases() {
		u16 count = 0;
#define DEF_OPCODE(opcode) count++;
#include "opcodes_list.hpp"
#undef DEF_OPCODE
		return count;
	}
}

namespace vm {
	class VMThread;

	/**
	 * @brief Bytecode instruction representation.
	 *
	 * Depends on the @ref VM/src/config.hpp configuration.
	 */
	struct Fix8Instruction;

	class OpFuns;
	using OpFun = void(OPFUN_ARGS);


	// Describes number of DuckBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	// (CG, executor.cpp)
	constexpr u16 OP_CASES_COUNT = countOpCases();

#ifdef USE_TAIL_CALLS
	struct Fix8Instruction {
		OpFun* opfun;
		i32    arg0;
		i32    arg1;
	};
#else
	#ifdef USE_COMPACT_INSTRUCTION
	struct Fix8Instruction {
		i64 opcode: 16, arg0: 24, arg1: 24;
	};
	#else
	struct Fix8Instruction {
		u16 opcode;
		i32 arg0;
		i32 arg1;
	};
	#endif
#endif

	/**
	 * @brief A class that contains all opcode functions implementations
	 * Executor service calls these functions to execute the instructions.
	 * For convenience they are implemented in the `executor.cpp` file.
	 */
	class OpFuns {
	public:
#define DEF_OPCODE(opcode) static OpFun op_##opcode;
#include "opcodes_list.hpp"
#undef DEF_OPCODE
		static OpFun handle_strategy;
		/**
		 * @brief A mapping between opcode ids and function pointers.
		 *
		 * @warning Ordering of elements must stay the same as in vm::OpcodeFix8
		 */
		static constexpr std::array<OpFun*, OP_CASES_COUNT> opfuns{
#define DEF_OPCODE(opcode) op_##opcode,
#include "opcodes_list.hpp"
#undef DEF_OPCODE
		};
	};
}  // namespace vm
