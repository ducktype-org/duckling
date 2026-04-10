#pragma once

#include <limits>
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

            calcPredecessors(cfg);
            calcDominators(cfg);

            for (BlockID bid = 0; bid < cfg.size(); ++bid) {
                // PLACEHOLDER IMPLEMENTATION
                for (BlockID pred : predecessors[bid]) {
                    if (isDominatedBy(pred, bid)) {
                        loops.emplace_back(cfg.getBlock(bid), cfg.getBlock(pred));
                    }
                }
            }

            return loops;
        }

      private:
        const BlockID UNDEFINED = std::numeric_limits<BlockID>::max();

        std::vector<bool> visited;
        std::vector<u32> postorder;
        std::vector<BlockID> invPostorderMap;
        std::vector<std::vector<BlockID>> predecessors;

        std::vector<BlockID> immDom;
        std::vector<std::vector<BlockID>> domTree;
        std::vector<std::pair<u32, u32>> domTreeTimestamps;

        void postorderDfs(const ControlFlowGraph& cfg, BlockID bid, u32& ctr) {
            visited[bid] = true;
            const BasicBlock& block = cfg.getBlock(bid);

            for (usize i = 0; i < block.edgeCount(); ++i) {
                BlockID target_id = block.edge(i);
                if (!visited[target_id]) {
                    postorderDfs(cfg, target_id, ctr);
                }
            }

            postorder[bid] = ctr;
            invPostorderMap[ctr++] = bid;
        }

        void calcPostorder(const ControlFlowGraph& cfg) {
            visited.assign(cfg.size(), false);
            u32 ctr = 0;
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

        BlockID intersect(BlockID b1, BlockID b2) const {
            while (b1 != b2) {
                while (postorder[b1] < postorder[b2]) {
                    b1 = immDom[b1];
                }
                while (postorder[b2] < postorder[b1]) {
                    b2 = immDom[b2];
                }
            }
            return b1;
        }

        void calcImmediateDominators(const ControlFlowGraph& cfg) {
            immDom.assign(cfg.size(), UNDEFINED);
            calcPostorder(cfg);

            immDom[0] = 0; // Entry block dominates itself
            bool changed = true;
            while (changed) {
                changed = false;
                for (usize i = cfg.size() - 1 ; i > 0; --i) {
                    BlockID b = invPostorderMap[i];
                    BlockID new_idom = predecessors[b][0];

                    for (usize j = 1; j < predecessors[b].size(); ++j) {
                        BlockID p = predecessors[b][j];
                        if (immDom[p] != UNDEFINED) {
                            new_idom = intersect(p, new_idom);
                        }
                    }

                    if (immDom[b] != new_idom) {
                        immDom[b] = new_idom;
                        changed = true;
                    }
                }
            }
        }

        void domTreeTimestampDfs(BlockID bid, u32& time) {
            domTreeTimestamps[bid].first = time++;
            for (BlockID child_id : domTree[bid]) {
                domTreeTimestampDfs(child_id, time);
            }
            domTreeTimestamps[bid].second = time++;
        }

        void calcDominators(const ControlFlowGraph& cfg) {
            calcImmediateDominators(cfg);

            domTree.assign(cfg.size(), std::vector<BlockID>());
            for (usize block_id = 1; block_id < cfg.size(); ++block_id) {
                BlockID idom = immDom[block_id];
                domTree[idom].push_back(block_id);
            }

            domTreeTimestamps.assign(cfg.size(), {0, 0});
            u32 time = 0;

            domTreeTimestampDfs(0, time); // Start DFS from the entry block
        }

        // Requires that dominators have already been calculated to work correctly
        bool isDominatedBy(BlockID bid, BlockID domid) const {
            return domTreeTimestamps[domid].first <= domTreeTimestamps[bid].first &&
                   domTreeTimestamps[bid].second <= domTreeTimestamps[domid].second;
        }
    };
} // namespace vm::jit::cf
