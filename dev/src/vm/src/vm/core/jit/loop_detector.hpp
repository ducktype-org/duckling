/**
 * @file loop_detector.hpp
 * @brief Natural loop detection based on CFG dominator analysis.
 */
#pragma once

#include "cf_graph.hpp"

#include <limits>
#include <vector>

namespace vm::jit::cf {
	class LoopDetector;

	/**
	 * @brief Closed instruction range representing a detected loop.
	 */
	class Loop {
	public:
		/**
		 * @brief Creates a loop from start and back-edge source blocks.
		 * @param start_block Loop header block.
		 * @param end_block Back-edge source block.
		 */
		Loop(const BasicBlock& start_block, const BasicBlock& end_block):
			  start_block(start_block),
			  end_block(end_block) {}

		Loop() = delete;

		/**
		 * @brief Returns inclusive bytecode index where the loop starts.
		 */
		[[nodiscard]] usize start() const { return start_block.start; }

		/**
		 * @brief Returns exclusive bytecode index where the loop ends.
		 */
		[[nodiscard]] usize end() const { return end_block.end; }

	private:
		const BasicBlock& start_block;
		const BasicBlock& end_block;

		friend class LoopDetector;
	};

	/**
	 * @brief Detects loops in control-flow graphs using dominator relations.
	 */
	class LoopDetector {
	public:
		LoopDetector() = default;

		/**
		 * @brief Detects loops in function CFG via back edges.
		 * @param cfg Control-flow graph.
		 * @return Detected loops represented as instruction ranges.
		 * @note Current implementation is marked as placeholder in code.
		 */
		std::vector<Loop> findLoops(const ControlFlowGraph& cfg) {
			std::vector<Loop> loops;

			calcPredecessors(cfg);
			calcDominators(cfg);

			for (BlockID bid = 0; bid < cfg.size(); ++bid) {
				// PLACEHOLDER IMPLEMENTATION
				for (BlockID pred: predecessors[bid])
					if (isDominatedBy(pred, bid))
						loops.emplace_back(cfg.getBlock(bid), cfg.getBlock(pred));
			}

			return loops;
		}

	private:
		/**
		 * @brief Sentinel value for undefined block identifiers.
		 */
		const BlockID undefined = std::numeric_limits<BlockID>::max();

		std::vector<bool>                 visited;
		std::vector<u32>                  postorder;
		std::vector<BlockID>              inv_postorder_map;
		std::vector<std::vector<BlockID>> predecessors;

		std::vector<BlockID>              imm_dom;
		std::vector<std::vector<BlockID>> dom_tree;
		std::vector<std::pair<u32, u32>>  dom_tree_timestamps;

		/**
		 * @brief DFS traversal assigning postorder indices.
		 * @param cfg Control-flow graph.
		 * @param bid Currently visited block.
		 * @param ctr Running postorder counter.
		 */
		void postorderDfs(const ControlFlowGraph& cfg, BlockID bid, u32& ctr) {
			visited[bid]            = true;
			const BasicBlock& block = cfg.getBlock(bid);

			for (usize i = 0; i < block.edgeCount(); ++i) {
				BlockID target_id = block.edge(i);
				if (!visited[target_id]) postorderDfs(cfg, target_id, ctr);
			}

			postorder[bid]           = ctr;
			inv_postorder_map[ctr++] = bid;
		}

		/**
		 * @brief Computes postorder numbering from the entry block.
		 * @param cfg Control-flow graph.
		 */
		void calcPostorder(const ControlFlowGraph& cfg) {
			visited.assign(cfg.size(), false);
			u32 ctr = 0;
			postorderDfs(cfg, 0, ctr);  // Start DFS from the entry block
		}

		/**
		 * @brief Computes predecessor list for every block.
		 * @param cfg Control-flow graph.
		 */
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

		/**
		 * @brief Intersects two dominator chains.
		 * @param b1 First block.
		 * @param b2 Second block.
		 * @return Nearest common dominator in current idom state.
		 */
		[[nodiscard]] BlockID intersect(BlockID b1, BlockID b2) const {
			while (b1 != b2) {
				while (postorder[b1] < postorder[b2]) b1 = imm_dom[b1];
				while (postorder[b2] < postorder[b1]) b2 = imm_dom[b2];
			}
			return b1;
		}

		/**
		 * @brief Computes immediate dominator for each reachable block.
		 * @param cfg Control-flow graph.
		 */
		void calcImmediateDominators(const ControlFlowGraph& cfg) {
			imm_dom.assign(cfg.size(), undefined);
			calcPostorder(cfg);

			imm_dom[0]   = 0;  // Entry block dominates itself
			bool changed = true;
			while (changed) {
				changed = false;
				for (usize i = cfg.size() - 1; i > 0; --i) {
					BlockID b        = inv_postorder_map[i];
					BlockID new_idom = predecessors[b][0];

					for (usize j = 1; j < predecessors[b].size(); ++j) {
						BlockID p = predecessors[b][j];
						if (imm_dom[p] != undefined) new_idom = intersect(p, new_idom);
					}

					if (imm_dom[b] != new_idom) {
						imm_dom[b] = new_idom;
						changed    = true;
					}
				}
			}
		}

		/**
		 * @brief Timestamps dominator tree with DFS in/out times.
		 * @param bid Current dominator tree node.
		 * @param time Running DFS timestamp.
		 */
		void domTreeTimestampDfs(BlockID bid, u32& time) {
			dom_tree_timestamps[bid].first = time++;
			for (BlockID child_id: dom_tree[bid]) domTreeTimestampDfs(child_id, time);
			dom_tree_timestamps[bid].second = time++;
		}

		/**
		 * @brief Builds dominator tree and dominance query timestamps.
		 * @param cfg Control-flow graph.
		 */
		void calcDominators(const ControlFlowGraph& cfg) {
			calcImmediateDominators(cfg);

			dom_tree.assign(cfg.size(), std::vector<BlockID>());
			for (usize block_id = 1; block_id < cfg.size(); ++block_id) {
				BlockID idom = imm_dom[block_id];
				dom_tree[idom].push_back(block_id);
			}

			dom_tree_timestamps.assign(cfg.size(), { 0, 0 });
			u32 time = 0;

			domTreeTimestampDfs(0, time);  // Start DFS from the entry block
		}

		/**
		 * @brief Checks whether one block dominates another.
		 * @param bid Candidate dominated block.
		 * @param domid Candidate dominator block.
		 * @return True if domid dominates bid.
		 * @note Requires dominator timestamps to be precomputed.
		 */
		[[nodiscard]] bool isDominatedBy(BlockID bid, BlockID domid) const {
			return dom_tree_timestamps[domid].first <= dom_tree_timestamps[bid].first
			    && dom_tree_timestamps[bid].second <= dom_tree_timestamps[domid].second;
		}
	};
}  // namespace vm::jit::cf
