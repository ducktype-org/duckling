/**
 * @file cf_analysis.hpp
 * @brief Control-flow analysis utilities for splitting lowered bytecode into basic blocks and CFGs.
 */
#pragma once

#include "../low_program.hpp"

#include <vm/bytecode/bytecode.hpp>

#include <bit>
#include <vector>

namespace vm::low::cf {
	/**
	 * @brief Computes absolute jump target from next-instruction offset and encoded delta.
	 * @param next_offset Offset of the instruction immediately after the jump.
	 * @param raw_delta Signed relative jump delta encoded in u64.
	 * @return Absolute jump target as instruction index.
	 */
	[[nodiscard]] inline usize jumpTarget(usize next_offset, u64 raw_delta) {
		const i64 target = static_cast<i64>(next_offset) + std::bit_cast<i64>(raw_delta);
		CORE_ASSERT(target >= 0, "Negative jump target computed, likely due to malformed bytecode");
		return static_cast<usize>(target);
	}

	/**
	 * @brief Finds instruction offsets where basic blocks start.
	 * @param bc Micro-bytecode of lowered function to analyze.
	 * @return Sorted list of basic-block beginnings.
	 */
	[[nodiscard]] std::vector<usize> basicBlockBeginnings(const low::MicroBytecode& bc);

	/**
	 * @brief Finds the first jitEntrypoint instruction in the bytecode.
	 * @param bc Micro-bytecode of lowered function to analyze.
	 * @return Offset of the function's jitEntrypoint instruction.
	 */
	[[nodiscard]] usize functionEntrypointOffset(const MicroBytecode& bc);
}  // namespace vm::low::cf
