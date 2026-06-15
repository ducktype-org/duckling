/**
 * @file cf_analysis.cpp
 * @brief Implementation of basic-block boundary analysis.
 */
#include "cf_analysis.hpp"

#include "../instruction.hpp"
#include "../opcodes.hpp"

#include <vm/bytecode/instructions.hpp>

#include <algorithm>

using namespace vm::code::instructions;

namespace vm::low::cf {
	/**
	 * @brief Computes start offsets of all basic blocks in lowered bytecode.
	 * @param bc Micro-bytecode of lowered function to analyze.
	 * @return Sorted offsets where each basic block begins.
	 */
	std::vector<usize> basicBlockBeginnings(const low::MicroBytecode& bc) {
		std::vector<usize> block_beginnings = { 0 };  // First block always starts at position 0

		for (usize index = 0; index < bc.size(); ++index) {
			low::MicroOpcode opcode = getInstructionOpcode(bc[index]);
			u64              arg0   = bc[index].arg0;

			switch (opcode) {
			case low::MicroOpcode::jmp_label:
			case low::MicroOpcode::jmpIf_label:
			case low::MicroOpcode::jmpIfNot_label: {
				const usize jump_target = jumpTarget(index + 1, arg0);

				block_beginnings.push_back(index + 1);        // Next block starts after jump
				if (jump_target < bc.size())
					block_beginnings.push_back(jump_target);  // Jump destination starts a new block
				break;
			}
			case low::MicroOpcode::ret:
			case low::MicroOpcode::ret_tailcall_func: {
				block_beginnings.push_back(index + 1);  // Next block starts after ret
				break;
			}
			default: {
				break;
			}
			}
		}

		// Remove duplicates (occur if there are many jumps to the same destination)
		std::ranges::sort(block_beginnings);
		block_beginnings.erase(
			std::ranges::unique(block_beginnings).begin(), block_beginnings.end()
		);

		if (block_beginnings.back() == bc.size()) {
			block_beginnings.pop_back(
			);  // Remove the last block beginning if it points to the end of the bytecode
		}
		return block_beginnings;
	}

	/**
	 * @brief Finds the first jitFunctionEntrypoint instruction in the bytecode.
	 * @param bc Micro-bytecode of lowered function to analyze.
	 * @return Offset of the function's jitFunctionEntrypoint instruction.
	 */
	usize functionEntrypointOffset(const MicroBytecode& bc) {
		for (usize i = 0; i < bc.size(); ++i)
			if (getInstructionOpcode(bc[i]) == MicroOpcode::jitFuncEntrypoint) return i;
		CORE_UNREACHABLE();
	}
}  // namespace vm::low::cf
