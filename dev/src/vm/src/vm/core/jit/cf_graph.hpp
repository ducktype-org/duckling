#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit::cf {
    using BlockID = usize;

    class OutEdges {
      public:
        enum class Kind {
            End,
            Default,
            JmpIf,
            JmpIfNot,
        };

        OutEdges(): to{0, 0}, size_(0), kind_(Kind::End) {}

        usize size() const {
            return size_;
        }

        Kind kind() const {
            return kind_;
        }

        void setCond(Kind kind, BlockID target1, BlockID target2) {
            if (size_ != 0) {
                throw std::runtime_error("Outgoing edges already set for this basic block");
            }
            this->kind_ = kind;
            to[0] = target1;
            to[1] = target2;
            size_ = 2;
        }

        void setDefault(BlockID target) {
            if (size_ != 0) {
                throw std::runtime_error("Outgoing edges already set for this basic block");
            }
            this->kind_ = Kind::Default;
            to[0] = target;
            size_ = 1;
        }

        BlockID next() const {
            if (kind_ != Kind::Default) {
                throw std::runtime_error("This block has no default outgoing edge.");
            }
            return to[0];
        }

        BlockID successTarget() const {
            if (kind_ == Kind::JmpIf || kind_ == Kind::JmpIfNot) {
                return to[0];
            }
            throw std::runtime_error("This block does not have conditional outgoing edges.");
        }

        BlockID failTarget() const {
            if (kind_ == Kind::JmpIf || kind_ == Kind::JmpIfNot) {
                return to[1];
            }
            throw std::runtime_error("This block does not have conditional outgoing edges.");
        }

        BlockID operator[](usize index) const {
            return to[index];
        }

      private:
        std::array<BlockID, 2> to;
        usize size_;
        Kind  kind_;
    };

    struct BasicBlock {
      private:
        OutEdges succ;

      public:
        const BlockID id;
        const usize start;
        const usize end;

        BasicBlock(BlockID id, usize start, usize end)
            : succ(), id(id), start(start), end(end) {}
        BasicBlock() = delete;

        OutEdges::Kind edgeKind() const {
            return succ.kind();
        }

        BlockID next() const {
            return succ.next();
        }

        BlockID successTarget() const {
            return succ.successTarget();
        }

        BlockID failTarget() const {
            return succ.failTarget();
        }

        void setCondEdge(OutEdges::Kind kind, BlockID target1, BlockID target2) {
            succ.setCond(kind, target1, target2);
        }

        void setDefaultEdge(BlockID target) {
            succ.setDefault(target);
        }

        BlockID edge(usize index) const {
            return succ[index];
        }
    };

    class ControlFlowGraph {
      private:
        std::vector<BasicBlock> blocks;

      public:
        ControlFlowGraph() = default;

        ControlFlowGraph(const low::LowFuncData& function, std::vector<usize> block_beginnings) {
            blocks.reserve(block_beginnings.size());

            auto instrToBlock = [block_beginnings](usize instr_index) {
                auto it = std::ranges::lower_bound(block_beginnings, instr_index);
                if (it == block_beginnings.end()) return block_beginnings.size() - 1;

                usize block_idx = std::distance(block_beginnings.begin(), it);
                return block_idx;
            };

            for (BlockID id = 0; id < block_beginnings.size() - 1; ++id) {
                usize start = block_beginnings[id];
                usize end = block_beginnings[id + 1];
                blocks.emplace_back(id, start, end);
            }

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

        usize size() const {
            return blocks.size();
        }

        const BasicBlock& getBlock(BlockID id) const {
            if (id >= blocks.size()) {
                throw std::runtime_error("Invalid block ID");
            }
            return blocks[id];
        }
    };
}