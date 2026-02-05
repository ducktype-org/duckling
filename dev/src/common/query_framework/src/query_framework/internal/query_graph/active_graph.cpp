#include "active_graph.hpp"

namespace query::internal {

	void ActiveGraph::putNode(NodeID node_id) {
		active_nodes.put(node_id, {});
		active_node_count++;
	}

	void ActiveGraph::removeNode(NodeID node_id) {
		auto was_removed = active_nodes.erase(node_id);

		if (was_removed)
			active_node_count--;
		else
			CORE_PANIC("Removing non-existing node from active graph");
	}

	u64 ActiveGraph::size() const { return active_node_count.load(); }

	void ActiveGraph::removeEdge(NodeID node_id) {
		// Note that there might be some concurrent operations
		// between following assertion and update,
		// but the assertion must always pass anyway (when the active graph is used correctly).
		CORE_ASSERT(
			!active_nodes.atMaybeCopy(node_id).value().active_edge.empty(),
			"Removing edge for node that does not have an active edge"
		);
		active_nodes.update(node_id, {});
	}

	void ActiveGraph::setEdge(NodeID node_id, NodeID edge) {
		// Note that there might be some concurrent operations
		// between following assertion and update,
		// but the assertion must always pass anyway (when the active graph is used correctly).
		CORE_ASSERT(
			active_nodes.atMaybeCopy(node_id).value().active_edge.empty(),
			"Setting edge for node that already has an active edge"
		);
		active_nodes.update(node_id, { edge });
	}

	base::Optional<NodeID> ActiveGraph::walk(NodeID node_id) const {
		auto edge = active_nodes.atMaybeCopy(node_id);
		if (edge.empty()) return {};
		return edge.value().active_edge;
	}

	base::Optional<ActiveGraph::QueryCycle> ActiveGraph::cycleCheck(const NodeID node_id) const {
		auto double_walk = [this](NodeID walk_zero) -> base::Optional<NodeID> {
			auto walk_one = walk(walk_zero);
			if (walk_one.empty()) return {};
			return walk(walk_one.value());
		};

		base::Optional<NodeID> current_node_slow = node_id;
		base::Optional<NodeID> current_node_fast = node_id;

		while (true) {
			current_node_slow = walk(current_node_slow.value());
			if (current_node_slow.empty()) return {};

			current_node_fast = double_walk(current_node_fast.value());
			if (current_node_fast.empty()) return {};

			if (current_node_slow.value() == current_node_fast.value()) break;
		}

		// We are here, so the cycle was found.
		// Now we need to reconstruct the cycle nodes.
		// Note that due to the assumptions on the active graph usage,
		// the found cycle cannot change while we reconstruct it,
		// as no nodes can be removed from the graph until their active edges
		// are "computed" and removed.

		std::vector<NodeID> cycle_nodes;
		bool                is_the_initial_node_on_the_cycle = false;

		NodeID cycle_start = current_node_slow.value();
		cycle_nodes.push_back(cycle_start);
		if (cycle_start == node_id) is_the_initial_node_on_the_cycle = true;

		NodeID walker = walk(cycle_start).value();
		while (walker != cycle_start) {
			cycle_nodes.push_back(walker);
			if (walker == node_id) is_the_initial_node_on_the_cycle = true;
			walker = walk(walker).value();
		}

		if (!is_the_initial_node_on_the_cycle) return {};

		return QueryCycle{ .cycle_nodes = std::move(cycle_nodes) };
	}
}
