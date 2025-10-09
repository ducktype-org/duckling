/**
 * @file instruction.hpp
 * @brief Defines the instruction structure (types) and the opcodes functions used by the Executor
 * and Parser modules.
 */
#pragma once

#include <base/ints.hpp>

#include <vm/core/process/memory/frame.hpp>

/**
 * OPFUN_TC_ARGS are OpFun arguments for the Tail Call version.
 * OPFUN_REF_ARGS are OpFun arguments for the non tail call version.
 * The difference is that the OPFUN_REF_ARGS are pointers passed by reference.
 * It is because the OpFun's can change the underlying pointers to point to something else,
 * like the next instruction or the new frame.
 * In switch case we call the OpFuns directly from the cases, so to see the changes in pointers
 * in the main switch function we need to pass the pointers by reference.
 */
#define OPFUN_TC_ARGS                                                                        \
	const MicroInstruction *instr [[maybe_unused]], std::byte *local_stack [[maybe_unused]], \
		Frame *frame [[maybe_unused]], VMThread &thread [[maybe_unused]]

#define OPFUN_REF_ARGS                                                                         \
	const MicroInstruction *&instr [[maybe_unused]], std::byte *&local_stack [[maybe_unused]], \
		Frame *&frame [[maybe_unused]]                                                         \
		,                                                                                      \
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
	struct MicroInstruction;

	using OpFunTC = void(OPFUN_TC_ARGS);

	// Describes number of DuckBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	// (CG, executor.cpp)
	constexpr u16 OP_CASES_COUNT = countOpCases();

	struct MicroInstruction {
		union {
			/**
			 * @brief Index indicating which opcode it is.
			 * @details This is used when the `USE_TAIL_CALLS` option is disabled.
			 */
			u64 nontc_opcode;

			/**
			 * @brief Pointer to the opcode functions
			 * @details This is used when the `USE_TAIL_CALLS` option is enabled.
			 */
			OpFunTC* tc_opfun;
		};

		u64 arg0;
		u64 arg1;

#if defined(BUILD_TYPE_DEV_DEBUG)
		u64         opcode_id = std::numeric_limits<u64>::max();
		std::string representation{};
#endif
	};

	/**
	 * @brief Creates a low-level instruction with correct "union" type depending on the config.
	 * @return MicroInstruction
	 */
	MicroInstruction makeLowInstruction(u64 opcode, u64 arg0 = 0, u64 arg1 = 0);

	/**
	 * @brief For main purposes only.
	 * Returns human-readable instruction config.
	 */
	std::string getInstructionConfig();
}
