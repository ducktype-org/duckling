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
            End,        // No outgoing edges (e.g., return)
            Default,    // Jmp or fallthrough
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

        usize edgeCount() const {
            return succ.size();
        }
    };

    class ControlFlowGraph {
      private:
        std::vector<BasicBlock> blocks;

        void createCFG(const low::LowFuncData& function, const std::vector<usize>& block_beginnings);

      public:
        ControlFlowGraph() = default;

        ControlFlowGraph(const low::LowFuncData& function, std::vector<usize> block_beginnings) {
            createCFG(function, block_beginnings);
        }

        usize size() const {
            return blocks.size();
        }

        const BasicBlock& getBlock(BlockID id) const {
            CORE_ASSERT(id >= blocks.size(), "Invalid block ID");
            return blocks[id];
        }
    };
} // vm::jit::cf
