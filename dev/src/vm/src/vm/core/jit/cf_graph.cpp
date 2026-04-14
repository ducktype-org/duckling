#include "cf_graph.hpp"

namespace vm::jit::cf {
    void ControlFlowGraph::createCFG(const low::LowFuncData& function, const std::vector<usize>& block_beginnings) {
        blocks.clear();
        blocks.reserve(block_beginnings.size());

        CORE_ASSERT(!block_beginnings.empty(), "There should be at least one block beginning for a valid function");

        auto instrToBlock = [block_beginnings](usize instr_index) {
            auto it = std::ranges::lower_bound(block_beginnings, instr_index);

            CORE_ASSERT(it != block_beginnings.end(), "Instruction index out of bounds for block beginnings");

            usize block_idx = std::distance(block_beginnings.begin(), it);
            return block_idx;
        };

        for (BlockID id = 0; id < block_beginnings.size() - 1; ++id) {
            usize start = block_beginnings[id];
            usize end = block_beginnings[id + 1];
            blocks.emplace_back(id, start, end);
        }
        blocks.emplace_back(block_beginnings.size() - 1, block_beginnings.back(), function.bc.size());

        for (const auto& block : blocks) {
            const vm::MicroInstruction& last_instr = function.bc[block.end - 1];
            low::MicroOpcode            last_opcode = getInstructionOpcode(last_instr);

            switch (last_opcode) {
            case low::MicroOpcode::jmp_label: {
                usize target_block_idx = instrToBlock(block.end + last_instr.arg0);
                blocks[block.id].setDefaultEdge(target_block_idx);
                break;
            }
            case low::MicroOpcode::jmpIf_label: {
                usize jmp_target = instrToBlock(block.end + last_instr.arg0);
                blocks[block.id].setCondEdge(OutEdges::Kind::JmpIf, jmp_target, block.id + 1);
                break;
            }
            case low::MicroOpcode::jmpIfNot_label: {
                usize jmp_target = instrToBlock(block.end + last_instr.arg0);
                blocks[block.id].setCondEdge(OutEdges::Kind::JmpIfNot, jmp_target, block.id + 1);
                break;
            }
            case low::MicroOpcode::ret:
            case low::MicroOpcode::ret_tailcall_func:{
                // No outgoing edges from return blocks
                break;
            }
            default: {
                if (block.end < function.bc.size()) {
                    // If the block does not end with a jump, add a default edge to the next block
                    blocks[block.id].setDefaultEdge(block.id + 1);
                }
                break;
            }
            }
        }
    }
} // vm::jit::cf
