#include "query_state.hpp"

#include <base/collections/maps.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <iostream>

#ifdef BUILD_TYPE_DEV
	#include <unordered_set>
#endif

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
}
