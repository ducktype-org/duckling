/**
 * @file instruction.hpp
 * @brief Defines the instruction structure (types) and the opcodes functions used by the Executor
 * and Parser modules.
 */
#pragma once

#include <base/ints.hpp>

#include <vm/core/process/memory/frame.hpp>

#define OPFUN_TC_ARGS                                                                       \
	const Fix8Instruction *instr [[maybe_unused]], std::byte *local_stack [[maybe_unused]], \
		Frame *frame [[maybe_unused]], VMThread &thread [[maybe_unused]]

#define OPFUN_REF_ARGS                                                                        \
	const Fix8Instruction *&instr [[maybe_unused]], std::byte *&local_stack [[maybe_unused]], \
		Frame *&frame [[maybe_unused]]                                                        \
		,                                                                                     \
		VMThread &thread [[maybe_unused]]


#define RETURN_TYPE_OPFUN_REF void
#define RETURN_TYPE_OPFUN_TC  void

namespace {
	/**
	 * @brief Returns number of opcodes recognized by Executor in a compile-time.
	 * Used for `vm::OP_CASES_COUNT`.
	 *
	 * @return constexpr u16
	 */
	constexpr u16 countOpCases() {
		u16 count = 0;
#define HANDLE_OPCODE(opcode) count++;
#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE
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

	using OpFunTC = void(OPFUN_TC_ARGS);

	// Describes number of DuckBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	// (CG, executor.cpp)
	constexpr u16 OP_CASES_COUNT = countOpCases();

	struct Fix8Instruction {
		union {
			u64      opcode;
			OpFunTC* opfun;
		};

		i32 arg0;
		i32 arg1;
	};

	/**
	 * @brief Creates a low-level instruction with correct "union" type depending on the config.
	 * @return Fix8Instruction
	 */
	Fix8Instruction makeLowInstruction(u64 opcode, i32 arg0 = 0, i32 arg1 = 0);
}
