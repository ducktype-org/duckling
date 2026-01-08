#include "query_state.hpp"

#include <base/collections/maps.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace query::internal {
	void QueryState::setEntry(NodeID node, NodeID from) {
		query_stack_size++;

		if (node_data.contains(node)) {
			if (node_data.at(node).color == Color::Visiting) {
				// Detect and print the cycle
				std::cerr << "Cycle detected in dependency graph: \n";
				NodeID              current = from;
				std::vector<NodeID> cycle;

				cycle.push_back(node);
				while (current != node && node_data.contains(current)) {
					cycle.push_back(current);
					current = node_data.at(current).parent;
				}
				cycle.push_back(node);

				query_graph.debugPrintNodes(cycle, std::cerr);
				throw base::NotYetImplemented("Query Cycle!");
			}
		}
		node_data.insert_or_assign(node, NodeData(Color::Visiting, from));
		query_graph.node_deps.insert_or_assign(node, std::vector<NodeID>{});
	}

	void QueryState::setExit(NodeID node) {
		CORE_ASSERT(query_stack_size > 0, "Query exit called on empty call stack");
		query_stack_size--;

		node_data.at(node).color = Color::Done;
	}

	base::Optional<base::CRef<QueryGraph>> QueryState::getPreviousGraph() const {
		if (!previous.has_value()) return base::Optional<base::CRef<QueryGraph>>{};
		return &previous.value().graph;
	}

	u64 QueryState::queryStackSize() const { return query_stack_size; }

	void QueryState::setPrevNodeColor(internal::NodeID node, PrevColor color) {
		CORE_ASSERT(previous.has_value(), "PreviousCompilation is not set when setting node color");
		previous->node_colors.insert_or_assign(node, color);
	}

	base::CRef<base::HashMap<NodeID, QueryState::PrevColor>> QueryState::getPreviousNodeColors(
	) const {
		CORE_ASSERT(
			previous.has_value(), "PreviousCompilation is not set when accessing node colors"
		);
		return &previous.value().node_colors;
	}

	void QueryState::setPreviousGraph(QueryGraph&& graph) {
		CORE_ASSERT(!previous.has_value(), "Previous graph is already set");
		previous.emplace(std::move(graph));
	}

	QueryState::PrevColor QueryState::redGreenSweep(NodeID start_node) {
		// measure time spent in red-green sweep:
		timer::AddToTime _(&total_red_green_sweep_time);

		// No previous compilation graph -> cannot decide incremental reuse, mark as needs recompute
		if (!previous.has_value()) return PrevColor::Red;

		const auto& prev_graph  = previous->graph;
		auto&       node_colors = previous->node_colors;

		// If the node does not exist in the previous graph -> needs recomputation
		if (!prev_graph.nodeExists(start_node)) return PrevColor::Red;

		// If color is already known for this node, return it
		if (auto it = node_colors.find(start_node); it != node_colors.end()) return it->second;

		// Iterative DFS (post-order) over previous graph starting from start_node.
		// A node becomes Green iff all its direct dependencies are Green; otherwise Red.
		// Leaf nodes without an assigned color are marked Green by default.
		struct Frame {
			NodeID node;
			usize  idx;  // next child index to process
		};

		std::vector<Frame> stack;

		IF_BUILD_TYPE_DEV(std::unordered_set<NodeID> in_stack);

		stack.push_back(Frame{ .node = start_node, .idx = 0 });

		// This is used to detect back-edges (cycles) in the previous graph
		// The previous graph should be acyclic, but we just check it to PANIC if not
		IF_BUILD_TYPE_DEV(in_stack.insert(start_node);)

		while (!stack.empty()) {
			auto& frame = stack.back();
			auto  node  = frame.node;

			// If already colored (via another path), just pop and continue
			if (node_colors.contains(node)) {
				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop_back();
				continue;
			}

			// At this point, node must exist in previous graph because its a child of an existing node
			CORE_ASSERT(prev_graph.node_deps.contains(node), "Node should exist in previous graph");

			const auto& deps = prev_graph.node_deps.at(node);

			// If node has no entry or no deps -> treat as leaf; mark Green if not colored yet
			// If node is not colored that means node is not input, so we can safely mark it Green
			if (deps.empty()) {
				node_colors.insert_or_assign(node, PrevColor::Green);

				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop_back();
				continue;
			}

			// Process children one by one ensuring post-order coloring
			if (frame.idx < deps.size()) {
				const NodeID& child = deps[frame.idx++];

				// If child's color is known already, continue to next child
				if (node_colors.contains(child)) continue;

				// Push child for processing
				stack.push_back(Frame{ .node = child, .idx = 0 });

				IF_BUILD_TYPE_DEV(
					auto instert_result = in_stack.insert(child); CORE_ASSERT(
						instert_result.second,
						"Cycle detected in previous query graph during red-green sweep"
					);
				)

				continue;
			}

			// All children processed. Determine this node's color from its direct dependencies.
			bool all_green = true;
			for (const auto& c: deps) {
				auto itc = node_colors.find(c);
				if (itc == node_colors.end() || itc->second != PrevColor::Green) {
					all_green = false;
					break;
				}
			}
			node_colors.insert_or_assign(node, all_green ? PrevColor::Green : PrevColor::Red);

			IF_BUILD_TYPE_DEV(in_stack.erase(node);)

			stack.pop_back();
		}

		return node_colors.at(start_node);
	}

	NodeID QueryState::remapUnstableOrUnregisteredNodes(NodeID node) {
		static base::VectorMap<QueryID, QueryID> old_to_new;

		if (node.q_id.registered() && node.q_id.getData().usesStableHashing()) return node;

		// Here the QueryID is either unregistered or uses unstable hashing, so we do remapping

		if (auto existing = old_to_new.atMaybe(node.q_id); existing.has_value())
			return { **existing, node.hash };

		QueryData dummy_query_data(
			QueryKind::Dummy, "Dummy from previous graph created during deserialization", {}
		);
		QueryID new_qid = registerQuery(dummy_query_data);
		old_to_new.put(node.q_id, new_qid);
		return { new_qid, node.hash };
	}

	void QueryState::mergePreviousGraphIntoCurrentGraph(NodeID start_node) {
		// measure time spent in graph merges:
		timer::AddToTime _(&total_graph_merge_time);


		// NodeID with unstable hash might have diferent ID and graph in previous graph
		// So merging from such NodeID is not allowed
		CORE_ASSERT(
			start_node.q_id.getData().usesStableHashing(),
			"Cannot merge previous graph starting from QueryID that does not have stable hash"
		);

		// If there is no previous compilation graph, there's nothing to merge
		if (!previous.has_value()) return;

		// If the start node exists in the current graph -> it's already merged or recomputed
		if (query_graph.node_deps.contains(start_node)) return;

		const auto& prev_graph = previous->graph;

		// If the start node does not exist in previous graph, nothing to merge
		if (!prev_graph.nodeExists(start_node)) return;

		// Iterative DFS to copy nodes and their dependencies from previous graph
		struct Frame {
			NodeID node;
		};

		std::vector<Frame> stack;

		stack.push_back(Frame{ .node = start_node });

		while (!stack.empty()) {
			const auto frame = stack.back();
			stack.pop_back();

			const NodeID node = frame.node;

			// If we already created this node in current graph, skip
			if (query_graph.node_deps.contains(node)) continue;

			// Insert the node with an empty dependency list first (ensures parent exists for
			// addDependency)
			query_graph.node_deps.emplace(node, std::vector<NodeID>{});

			// Retrieve dependencies from previous graph; if none -> it's a leaf, keep empty deps
			CORE_ASSERT(
				prev_graph.node_deps.contains(node), "Node to merge should exist in previous graph"
			);
			const auto& prev_deps = prev_graph.node_deps.at(node);

			// For each child, add the dependency edge and ensure the child will be processed
			for (const auto& child: prev_deps) {
				// Add edge in current graph (child doesn't have to exist yet)
				query_graph.addDependency(node, child);

				// If child is not in current graph yet, schedule it for creation
				if (!query_graph.node_deps.contains(child)) stack.push_back(Frame{ .node = child });
			}
		}
	}

	void QueryState::reduceOptimizeGraph() {
		auto& deps = query_graph.node_deps;
		if (deps.empty()) return;

		base::HashMap<NodeID, std::vector<NodeID>> parents;
		parents.reserve(deps.size());
		for (const auto& [node, child_list]: deps) {
			parents.try_emplace(node, std::vector<NodeID>{});
			for (const auto& child: child_list) {
				auto [it, _] = parents.try_emplace(child, std::vector<NodeID>{});
				it->second.push_back(node);
			}
		}

		const auto isStableNode = [&](const NodeID& node) -> bool {
			return node.q_id.registered() && node.q_id.getData().usesStableHashing();
		};

		const auto isInputNode = [&](const NodeID& node) -> bool {
			return node.q_id.registered() && node.q_id.getData().isInputQuery();
		};

		const auto canTrimNode = [&](const NodeID& node) -> bool {
			if (isStableNode(node)) return false;
			if (isInputNode(node)) return false;
			return true;
		};

		// First optimization: remove unstable roots iteratively until only stable roots remain.
		std::vector<NodeID> root_queue;
		root_queue.reserve(deps.size());
		for (const auto& [node, _]: deps) {
			const auto parents_it = parents.find(node);
			if (parents_it != parents.end() && parents_it->second.empty() && canTrimNode(node))
				root_queue.push_back(node);
		}

		while (!root_queue.empty()) {
			const NodeID node = root_queue.back();
			root_queue.pop_back();

			auto dep_it = deps.find(node);
			if (dep_it == deps.end()) continue;
			auto parents_it = parents.find(node);
			if (parents_it != parents.end() && !parents_it->second.empty()) continue;

			for (const auto& child: dep_it->second) {
				auto child_parents_it = parents.find(child);
				if (child_parents_it == parents.end()) continue;
				auto& vec = child_parents_it->second;
				vec.erase(std::remove(vec.begin(), vec.end(), node), vec.end());
				if (vec.empty() && canTrimNode(child)) root_queue.push_back(child);
			}

			deps.erase(node);
			parents.erase(node);
		}

		std::vector<NodeID> leaves;
		leaves.reserve(deps.size());
		for (const auto& [node, child_list]: deps)
			if (child_list.empty() && canTrimNode(node)) leaves.push_back(node);

		std::vector<NodeID> next_leaves;
		std::vector<NodeID> dirty_parents;
		std::unordered_set<NodeID> leaves_batch;
		next_leaves.reserve(deps.size());
		dirty_parents.reserve(deps.size());

		while (!leaves.empty()) {
			leaves_batch.clear();
			dirty_parents.clear();
			leaves_batch.reserve(leaves.size() * 2);

			for (const auto& leaf: leaves) {
				if (!deps.contains(leaf)) continue;
				leaves_batch.insert(leaf);
				if (auto it = parents.find(leaf); it != parents.end())
					dirty_parents.insert(dirty_parents.end(), it->second.begin(), it->second.end());
			}

			if (leaves_batch.empty()) break;

			std::sort(dirty_parents.begin(), dirty_parents.end());
			dirty_parents.erase(std::unique(dirty_parents.begin(), dirty_parents.end()), dirty_parents.end());

			next_leaves.clear();
			for (const auto& parent: dirty_parents) {
				auto parent_it = deps.find(parent);
				if (parent_it == deps.end()) continue;

				auto& dep_vec = parent_it->second;
				auto  new_end = std::remove_if(dep_vec.begin(), dep_vec.end(), [&](const NodeID& dep) {
					return leaves_batch.contains(dep);
				});
				if (new_end != dep_vec.end()) {
					dep_vec.erase(new_end, dep_vec.end());
					if (dep_vec.empty() && canTrimNode(parent)) next_leaves.push_back(parent);
				}
			}

			for (const auto& leaf: leaves_batch) {
				deps.erase(leaf);
				parents.erase(leaf);
			}

			leaves.swap(next_leaves);
		}

		std::vector<NodeID> collapse_candidates;
		collapse_candidates.reserve(deps.size());
		for (const auto& [node, _]: deps) {
			if (auto it = parents.find(node); it != parents.end()) {
				auto& parent_vec = it->second;
				parent_vec.erase(std::remove_if(parent_vec.begin(), parent_vec.end(), [&](const NodeID& parent) {
					return !deps.contains(parent);
				}), parent_vec.end());
				if (parent_vec.size() == 1 && canTrimNode(node)) collapse_candidates.push_back(node);
			}
		}

		std::vector<NodeID> chain;
		std::vector<NodeID> new_children;
		std::unordered_set<NodeID> chain_nodes;
		chain.reserve(8);
		new_children.reserve(8);

		while (!collapse_candidates.empty()) {
			NodeID start = collapse_candidates.back();
			collapse_candidates.pop_back();

			if (!deps.contains(start)) continue;

			auto parents_entry = parents.find(start);
			if (parents_entry == parents.end()) continue;
			auto& start_parents = parents_entry->second;
			start_parents.erase(std::remove_if(start_parents.begin(), start_parents.end(), [&](const NodeID& candidate) {
				return !deps.contains(candidate);
			}), start_parents.end());

			if (start_parents.size() != 1) continue;
			if (!canTrimNode(start)) continue;

			NodeID anchor_candidate = start_parents.front();
			if (!deps.contains(anchor_candidate)) continue;

			chain.clear();
			chain.push_back(start);

			NodeID current_parent = anchor_candidate;
			while (true) {
				if (!canTrimNode(current_parent)) break;
				auto parent_it = parents.find(current_parent);
				if (parent_it == parents.end()) break;

				auto& parent_vec = parent_it->second;
				parent_vec.erase(std::remove_if(parent_vec.begin(), parent_vec.end(), [&](const NodeID& candidate) {
					return !deps.contains(candidate);
				}), parent_vec.end());

				if (parent_vec.size() != 1) break;
				chain.push_back(current_parent);
				current_parent = parent_vec.front();
				if (!deps.contains(current_parent)) break;
			}

			NodeID anchor = current_parent;
			if (!deps.contains(anchor)) continue;

			chain_nodes.clear();
			chain_nodes.reserve(chain.size() * 2);
			for (const auto& node: chain) chain_nodes.insert(node);

			new_children.clear();
			for (const auto& node: chain) {
				auto node_it = deps.find(node);
				if (node_it == deps.end()) continue;
				for (const auto& child: node_it->second) {
					if (chain_nodes.contains(child)) continue;
					new_children.push_back(child);
					if (auto child_parents_it = parents.find(child); child_parents_it != parents.end()) {
						auto& vec = child_parents_it->second;
						vec.erase(std::remove(vec.begin(), vec.end(), node), vec.end());
					}
				}
			}

			std::sort(new_children.begin(), new_children.end());
			new_children.erase(std::unique(new_children.begin(), new_children.end()), new_children.end());

			auto anchor_it = deps.find(anchor);
			if (anchor_it == deps.end()) continue;
			auto& anchor_deps = anchor_it->second;
			anchor_deps.erase(
				std::remove_if(anchor_deps.begin(), anchor_deps.end(), [&](const NodeID& child) {
					return chain_nodes.contains(child);
				}),
				anchor_deps.end()
			);

			std::unordered_set<NodeID> anchor_child_set(anchor_deps.begin(), anchor_deps.end());
			for (const auto& child: new_children) {
				if (anchor_child_set.insert(child).second) {
					anchor_deps.push_back(child);
					auto [child_parents_it, _] = parents.try_emplace(child, std::vector<NodeID>{});
					auto& vec = child_parents_it->second;
					if (std::find(vec.begin(), vec.end(), anchor) == vec.end()) vec.push_back(anchor);
				}
			}

			for (const auto& node: chain) {
				deps.erase(node);
				parents.erase(node);
			}

			for (const auto& child: new_children) {
				auto child_parents_it = parents.find(child);
				if (child_parents_it == parents.end()) continue;
				auto& vec = child_parents_it->second;
				vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const NodeID& candidate) {
					return !deps.contains(candidate);
				}), vec.end());
				if (vec.size() == 1 && canTrimNode(child)) collapse_candidates.push_back(child);
			}
		}
	}

}  // namespace query::internal
