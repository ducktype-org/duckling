#pragma once

#include <vector>

#include "cf_graph.hpp"

namespace vm::jit::cf {
    class LoopDetector;

    class Loop {
      public:
        Loop(const BasicBlock& startBlock, const BasicBlock& endBlock)
            : startBlock(startBlock), endBlock(endBlock) {}
        Loop() = delete;

        usize start() const {
            return startBlock.start;
        }

        usize end() const {
            return endBlock.end;
        }

      private:
        const BasicBlock& startBlock;
        const BasicBlock& endBlock;

        friend class LoopDetector;
    };

    class LoopDetector {
      public:
        LoopDetector() = default;

        std::vector<Loop> findLoops(const ControlFlowGraph& cfg) {
            std::vector<Loop> loops;

            calcDominators(cfg);

            for (usize block_id = 0; block_id < cfg.size(); ++block_id) {
                const auto& block = cfg.getBlock(block_id);
                processBlock(block, loops);
            }

            return loops;
        }

      private:
        std::vector<bool> visited;
        std::vector<usize> postorder;
        std::vector<BlockID> postorderMap;
        std::vector<std::vector<BlockID>> predecessors;

        std::vector<BlockID> immDom;

        void postorderDfs(const ControlFlowGraph& cfg, BlockID bid, usize& ctr) {
            visited[bid] = true;
            const auto& block = cfg.getBlock(bid);
            for (usize i = 0; i < block.edgeCount(); ++i) {
                BlockID target_id = block.edge(i);
                if (!visited[target_id]) {
                    postorderDfs(cfg, target_id, ctr);
                }
            }
            postorder[bid] = ctr;
            postorderMap[ctr++] = bid;
        }

        void calcPostorder(const ControlFlowGraph& cfg) {
            visited.assign(cfg.size(), false);
            usize ctr = 0;
            postorderDfs(cfg, 0, ctr); // Start DFS from the entry block
        }

        void calcPredecessors(const ControlFlowGraph& cfg) {
            predecessors.assign(cfg.size(), std::vector<BlockID>());

            for (usize block_id = 0; block_id < cfg.size(); ++block_id) {
                const BasicBlock& block = cfg.getBlock(block_id);
                for (usize i = 0; i < block.edgeCount(); ++i) {
                    BlockID target_id = block.edge(i);
                    predecessors[target_id].push_back(block_id);
                }
            }
        }

        void calcImmediateDominators(const ControlFlowGraph& cfg) {
            immDom.assign(cfg.size(), -1);
            calcPostorder(cfg);
            calcPredecessors(cfg);
            // Implementation for calculating immediate dominators
        }
    };
} // namespace vm::jit::cf
