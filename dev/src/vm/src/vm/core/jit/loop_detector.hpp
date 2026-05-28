/**
 * @file loop_detector.hpp
 * @brief Natural loop detection based on CFG dominator analysis.
 */
#pragma once

#include <vm/core/safe/low_program/cfg/cf_graph.hpp>

#include <limits>
#include <vector>

namespace vm::jit::cf {
	using vm::low::cf::BasicBlock;
	using vm::low::cf::BasicBlockID;
	using vm::low::cf::ControlFlowGraph;

	class LoopDetector;

	/**
	 * @brief Aggregated function CFG and detected loop CFGs.
	 * @details Index 0 contains the full function CFG; later entries contain loop subgraphs.
	 */
	struct FunctionLoopCFGs {
		std::vector<BasicBlockID>     start_blocks;
		std::vector<ControlFlowGraph> cfgs;
	};

	/**
	 * @brief Detects loops in control-flow graphs using dominator relations.
	 */
	class LoopDetector {
	public:
		LoopDetector() = default;

		/**
		 * @brief Detects natural loops in a function CFG using dominator relations.
		 * @return A function-level CFG at index 0 and loop CFGs at later indices.
		 */
		FunctionLoopCFGs findLoops(const low::MicroBytecode& bc) {
			ControlFlowGraph cfg(bc);
			FunctionLoopCFGs segs{};

			// First instruction is JIT entry point for entire function so we add full CFG as the
			// first segment (to avoid copying, we move it after creating all of the subgraphs,
			// here we just add an empty placeholder).
			segs.cfgs.push_back({});
			segs.start_blocks.push_back(0);

			calcPredecessors(cfg);
			calcDominators(cfg);

			std::vector<u32>          last_visited(cfg.size(), 0);
			u32                       timestamp = 0;
			std::vector<BasicBlockID> stack;
			usize                     stack_ptr = 0;

			for (BasicBlockID bid = 0; bid < cfg.size(); ++bid) {
				stack.clear();
				stack_ptr = 0;
				++timestamp;

				for (BasicBlockID pred: predecessors[bid]) {
					if (isDominatedBy(pred, bid)) {
						last_visited[pred] = timestamp;
						stack.push_back(pred);
					}
				}

				while (stack_ptr < stack.size()) {
					BasicBlockID current = stack[stack_ptr++];
					if (current == bid) continue;

					for (const auto& next: predecessors[current]) {
						if (last_visited[next] < timestamp) {
							stack.push_back(next);
							last_visited[next] = timestamp;
						}
					}
				}

				if (!stack.empty()) {
					CORE_ASSERT(bid != 0, "Entry block cannot be a loop header");
					segs.cfgs.push_back(cfg.subgraph(std::move(stack)));
					segs.start_blocks.push_back(bid);
				}
			}

			// Move full function CFG here to avoid copying it before all the subgraphs are created.
			segs.cfgs[0] = std::move(cfg);
			return segs;
		}

	private:
		/**
		 * @brief Sentinel value for undefined block identifiers.
		 */
		const BasicBlockID undefined = std::numeric_limits<BasicBlockID>::max();

		std::vector<bool>                      visited;
		std::vector<u32>                       postorder;
		std::vector<BasicBlockID>              inv_postorder_map;
		std::vector<std::vector<BasicBlockID>> predecessors;

		std::vector<BasicBlockID>              imm_dom;
		std::vector<std::vector<BasicBlockID>> dom_tree;
		std::vector<std::pair<u32, u32>>       dom_tree_timestamps;

		/**
		 * @brief DFS traversal assigning postorder indices.
		 * @param cfg Control-flow graph.
		 * @param bid Currently visited block.
		 * @param ctr Running postorder counter.
		 */
		void postorderDfs(const ControlFlowGraph& cfg, BasicBlockID bid, u32& ctr) {
			visited[bid]            = true;
			const BasicBlock& block = cfg.getBlock(bid);

			for (usize i = 0; i < block.edgeCount(); ++i) {
				BasicBlockID target_id = block.edge(i);
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
			postorder.assign(cfg.size(), 0);
			inv_postorder_map.assign(cfg.size(), 0);
			u32 ctr = 0;
			postorderDfs(cfg, 0, ctr);  // Start DFS from the entry block
		}

		/**
		 * @brief Computes predecessor list for every block.
		 * @param cfg Control-flow graph.
		 */
		void calcPredecessors(const ControlFlowGraph& cfg) {
			predecessors.assign(cfg.size(), std::vector<BasicBlockID>());

			for (usize block_id = 0; block_id < cfg.size(); ++block_id) {
				const BasicBlock& block = cfg.getBlock(block_id);
				for (usize i = 0; i < block.edgeCount(); ++i) {
					BasicBlockID target_id = block.edge(i);
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
		[[nodiscard]] BasicBlockID intersect(BasicBlockID b1, BasicBlockID b2) const {
			while (b1 != b2) {
				while (postorder[b1] < postorder[b2]) b1 = imm_dom[b1];
				while (postorder[b2] < postorder[b1]) b2 = imm_dom[b2];
			}
			return b1;
		}

		/**
		 * @brief Computes immediate dominator for each reachable block.
		 * @param cfg Control-flow graph.
		 * @note Assumes the only block without a predecessor is the entry block (id 0)
		 * 		 and that every other block is reachable from it.
		 */
		void calcImmediateDominators(const ControlFlowGraph& cfg) {
			imm_dom.assign(cfg.size(), undefined);
			calcPostorder(cfg);

			imm_dom[0]   = 0;  // Entry block dominates itself
			bool changed = true;
			while (changed) {
				changed = false;
				for (usize i = cfg.size() - 1; i > 0; --i) {
					BasicBlockID b = inv_postorder_map[i];
					CORE_ASSERT(
						!predecessors[b].empty(), "All blocks except entry should have predecessors"
					);
					BasicBlockID new_idom = predecessors[b][0];

					for (usize j = 1; j < predecessors[b].size(); ++j) {
						BasicBlockID p = predecessors[b][j];
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
		void domTreeTimestampDfs(BasicBlockID bid, u32& time) {
			dom_tree_timestamps[bid].first = time++;
			for (BasicBlockID child_id: dom_tree[bid]) domTreeTimestampDfs(child_id, time);
			dom_tree_timestamps[bid].second = time++;
		}

		/**
		 * @brief Builds dominator tree and dominance query timestamps.
		 * @param cfg Control-flow graph.
		 */
		void calcDominators(const ControlFlowGraph& cfg) {
			calcImmediateDominators(cfg);

			dom_tree.assign(cfg.size(), std::vector<BasicBlockID>());
			for (usize block_id = 1; block_id < cfg.size(); ++block_id) {
				BasicBlockID idom = imm_dom[block_id];
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
		[[nodiscard]] bool isDominatedBy(BasicBlockID bid, BasicBlockID domid) const {
			return dom_tree_timestamps[domid].first <= dom_tree_timestamps[bid].first
			    && dom_tree_timestamps[bid].second <= dom_tree_timestamps[domid].second;
		}
	};

	FunctionLoopCFGs detectLoopsInFunction(const low::MicroBytecode& bc) {
		LoopDetector detector;
		return detector.findLoops(bc);
	}
}  // namespace vm::jit::cf
