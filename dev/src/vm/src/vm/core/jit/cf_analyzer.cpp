/**
 * @file cf_analyzer.cpp
 * @brief Implementation of basic-block boundary analysis.
 */
#include "cf_analyzer.hpp"

#include <vm/bytecode/instructions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>

#include <algorithm>

using namespace vm::code::instructions;

namespace vm::jit::cf {
	/**
	 * @brief Computes start offsets of all basic blocks in lowered bytecode.
	 * @param function Lowered function to analyze.
	 * @return Sorted offsets where each basic block begins.
	 */
	std::vector<usize> ControlFlowAnalyzer::basicBlockBeginnings(const low::LowFuncData& function) {
		std::vector<usize> block_beginnings = { 0 };  // First block always starts at position 0

		for (usize index = 0; index < function.bc.size(); ++index) {
			low::MicroOpcode opcode = getInstructionOpcode(function.bc[index]);
			u64              arg0   = function.bc[index].arg0;

			switch (opcode) {
			case low::MicroOpcode::jmp_label:
			case low::MicroOpcode::jmpIf_label:
			case low::MicroOpcode::jmpIfNot_label: {
				const usize jump_target = jumpTarget(index + 1, arg0);

				block_beginnings.push_back(index + 1);        // Next block starts after jump
				if (jump_target < function.bc.size())
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

		if (block_beginnings.back() == function.bc.size()) {
			block_beginnings.pop_back(
			);  // Remove the last block beginning if it points to the end of the bytecode
		}
		return block_beginnings;
	}
}  // namespace vm::jit
