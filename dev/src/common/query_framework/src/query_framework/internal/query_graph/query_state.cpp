#include "query_state.hpp"

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

	bool QueryState::redGreenSweep(NodeID start_node) {
		// No previous compilation graph -> cannot decide incremental reuse, mark as needs recompute
		if (!previous.has_value()) return false;

		const auto& prev_graph  = previous->graph;
		auto&       node_colors = previous->node_colors;

		// If the node does not exist in the previous graph -> needs recomputation
		if (!prev_graph.nodeExists(start_node)) return false;

		// If color is already known for this node, return it
		if (auto it = node_colors.find(start_node); it != node_colors.end())
			return it->second == PrevColor::Green;

		// Iterative DFS (post-order) over previous graph starting from start_node.
		// A node becomes Green iff all its direct dependencies are Green; otherwise Red.
		// Leaf nodes without an assigned color are marked Green by default.
		struct Frame {
			NodeID node;
			usize  idx;  // next child index to process
		};

		std::vector<Frame>         stack;
		std::unordered_set<NodeID> in_stack;
		stack.push_back(Frame{ .node = start_node, .idx = 0 });

		// This is used to detect back-edges (cycles) in the previous graph
		// The previous graph should be acyclic, but we just check it to PANIC if not
		in_stack.insert(start_node);

		while (!stack.empty()) {
			auto& frame = stack.back();
			auto  node  = frame.node;

			// If already colored (via another path), just pop and continue
			if (node_colors.contains(node)) {
				in_stack.erase(node);
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
				in_stack.erase(node);
				stack.pop_back();
				continue;
			}

			// Process children one by one ensuring post-order coloring
			if (frame.idx < deps.size()) {
				const NodeID& child = deps[frame.idx++];

				// If child's color is known already, continue to next child
				if (node_colors.contains(child)) continue;

				if (in_stack.contains(child))
					CORE_PANIC("Cycle detected in previous query graph during red-green sweep");

				// Push child for processing
				stack.push_back(Frame{ .node = child, .idx = 0 });
				in_stack.insert(child);
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
			in_stack.erase(node);
			stack.pop_back();
		}

		return node_colors.at(start_node) == PrevColor::Green;
	}
}
