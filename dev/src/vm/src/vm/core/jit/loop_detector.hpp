#pragma once

#include "cf_graph.hpp"

#include <limits>
#include <vector>

namespace vm::jit::cf {
	class LoopDetector;

	struct Loop {
		Loop(BlockID start_block, BlockID end_block, ControlFlowGraph cfg, std::vector<BlockID> members):
			  start_block(start_block), end_block(end_block), loop_cfg(cfg.subgraph(members)) {}
		BlockID		  start_block;
		BlockID 		end_block;
		ControlFlowGraph loop_cfg;
	};

	class LoopDetector {
	public:
		LoopDetector() = default;

		std::vector<Loop> findLoops(const ControlFlowGraph& cfg) {
			std::vector<Loop> loops;

			calcPredecessors(cfg);
			calcDominators(cfg);

			std::vector<u32> last_visited(cfg.size(), 0);
			u32 timestamp = 0;
			std::vector<BlockID> stack;
			usize stack_ptr;

			for (BlockID bid = 0; bid < cfg.size(); ++bid) {
				for (BlockID pred: predecessors[bid]) {
					if (!isDominatedBy(pred, bid))
						continue;

					last_visited[bid] = ++timestamp;
					stack = { bid };
					stack_ptr = 1;
					while (stack_ptr < stack.size()) {
						BlockID current = stack[stack_ptr++];
						if (current == pred)
							continue; // This is a stop condition, because pred dominates bid

						for (usize i = 0; i < predecessors[current].size(); ++i) {
							BlockID next = predecessors[current][i];
							if (last_visited[next] < bid) {
								stack.push_back(next);
								last_visited[next] = timestamp;
							}
						}
					}
					loops.emplace_back(pred, bid, cfg, std::move(stack));
				}
			}

			return loops;
		}

	private:
		const BlockID undefined = std::numeric_limits<BlockID>::max();

		std::vector<bool>                 visited;
		std::vector<u32>                  postorder;
		std::vector<BlockID>              inv_postorder_map;
		std::vector<std::vector<BlockID>> predecessors;

		std::vector<BlockID>              imm_dom;
		std::vector<std::vector<BlockID>> dom_tree;
		std::vector<std::pair<u32, u32>>  dom_tree_timestamps;

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

		void calcPostorder(const ControlFlowGraph& cfg) {
			visited.assign(cfg.size(), false);
			u32 ctr = 0;
			postorderDfs(cfg, 0, ctr);  // Start DFS from the entry block
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

		[[nodiscard]] BlockID intersect(BlockID b1, BlockID b2) const {
			while (b1 != b2) {
				while (postorder[b1] < postorder[b2]) b1 = imm_dom[b1];
				while (postorder[b2] < postorder[b1]) b2 = imm_dom[b2];
			}
			return b1;
		}

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

		void domTreeTimestampDfs(BlockID bid, u32& time) {
			dom_tree_timestamps[bid].first = time++;
			for (BlockID child_id: dom_tree[bid]) domTreeTimestampDfs(child_id, time);
			dom_tree_timestamps[bid].second = time++;
		}

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

		// Requires that dominators have already been calculated to work correctly
		[[nodiscard]] bool isDominatedBy(BlockID bid, BlockID domid) const {
			return dom_tree_timestamps[domid].first <= dom_tree_timestamps[bid].first
			    && dom_tree_timestamps[bid].second <= dom_tree_timestamps[domid].second;
		}
	};
}  // namespace vm::jit::cf
