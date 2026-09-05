/**
 * @file instruction.hpp
 * @brief Defines the instruction structure (types) and the opcodes functions used by the Executor
 * and Parser modules.
 */
#pragma once

#include "opcodes.hpp"

#include <base/types/ints.hpp>

#include <vm/core/safe/memory/frame.hpp>

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
	[[maybe_unused]] const MicroInstruction *instr, [[maybe_unused]] std::byte *local_stack, \
		[[maybe_unused]] Frame *frame, [[maybe_unused]] SafeVMThread &thread

// There is a strong dependency in creating the instruction implementation type in LLVM JIT
// compiler. (jit_data.cpp)
#define OPFUN_REF_ARGS                                                                         \
	[[maybe_unused]] const MicroInstruction *&instr, [[maybe_unused]] std::byte *&local_stack, \
		[[maybe_unused]] Frame *&frame, [[maybe_unused]] SafeVMThread &thread


#define RETURN_TYPE_OPFUN_REF void
#define RETURN_TYPE_OPFUN_TC  void

namespace internal {
	/**
	 * @brief Returns number of opcodes recognized by Executor in a compile-time.
	 * Used for `vm::OP_CASES_COUNT`.
	 *
	 * @return constexpr u64
	 */
	constexpr u64 countOpCases() {
		u64 count = 0;
#define HANDLE_MICRO_INSTR(opcode) count++;
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>
#undef HANDLE_MICRO_INSTR
		return count;
	}
}

namespace vm {

	class SafeVMThread;

	/**
	 * @brief Bytecode instruction representation.
	 *
	 * Depends on the @ref VM/src/config.hpp configuration.
	 */
	struct MicroInstruction;

	using OpFunTC = void(OPFUN_TC_ARGS);

	// Describes number of DuckBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	constexpr u64 OP_CASES_COUNT = ::internal::countOpCases();

	struct MicroInstruction final {
#ifdef USE_TAIL_CALLS
		/**
		 * @brief Pointer to the opcode functions
		 * @details This is used when the `USE_TAIL_CALLS` option is enabled.
		 */
		OpFunTC* tc_opfun;
#else
		/**
		 * @brief Index indicating which opcode it is.
		 * @details This is used when the `USE_TAIL_CALLS` option is disabled.
		 */
		u64 nontc_opcode;
#endif
		u64 arg0;
		u64 arg1;

#if defined(BUILD_TYPE_DEV_DEBUG)
		low::MicroOpcode opcode_id = low::MicroOpcode{ std::numeric_limits<u64>::max() };
		std::string      representation{};
#endif
	};

	/**
	 * @brief Creates a low-level instruction with correct "union" type depending on the config.
	 * @return MicroInstruction
	 */
	MicroInstruction makeLowInstruction(low::MicroOpcode opcode, u64 arg0 = 0, u64 arg1 = 0);

	low::MicroOpcode getInstructionOpcode(const MicroInstruction& instruction);

	/**
	 * @brief For main purposes only.
	 * Returns human-readable instruction config.
	 */
	std::string getInstructionConfig();
}
