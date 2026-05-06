#include "active_graph.hpp"

namespace query::internal {

	void ActiveGraph::putNode(NodeID node_id, Ref<query::Context> node_context_ref) {
		active_nodes.put(
			node_id,
			{ .active_edge = base::Optional<NodeID>(), .node_context_ref = node_context_ref }
		);
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
		// Note that callOn will panic here, on a node that does not exist, this is the expected
		// behavior.
		active_nodes.callOn(node_id, [](Ref<ActiveData> data_ref) {
			CORE_ASSERT(
				data_ref->active_edge.has_value(),
				"Removing edge for node that does not have an active edge"
			);
			data_ref->active_edge.reset();
		});
	}

	void ActiveGraph::setEdge(NodeID node_id, NodeID edge) {
		// Note that callOn will panic here, on a node that does not exist, this is the expected
		// behavior.
		active_nodes.callOn(node_id, [edge](Ref<ActiveData> data_ref) {
			CORE_ASSERT(
				data_ref->active_edge.empty(),
				"Setting edge for node that already has an active edge"
			);
			data_ref->active_edge = edge;
		});
	}

	base::Optional<ActiveGraph::QueryCycle> ActiveGraph::cycleCheck(const NodeID initial_node_id
	) const {
		/***********************************************************\
		| Cycle detection algorithm: Floyd's Tortoise and Hare.     |
		\***********************************************************/

		// Walks a single edge in the active graph.
		auto walk = [this](NodeID node_id) -> base::Optional<NodeID> {
			auto edge = active_nodes.atMaybeCopy(node_id);
			if (edge.empty()) return {};
			return edge.value().active_edge;
		};

		auto double_walk = [walk](NodeID walk_zero) -> base::Optional<NodeID> {
			auto walk_one = walk(walk_zero);
			if (walk_one.empty()) return {};
			return walk(walk_one.value());
		};

		base::Optional<NodeID> current_node_slow = initial_node_id;
		base::Optional<NodeID> current_node_fast = initial_node_id;

		while (true) {
			current_node_slow = walk(current_node_slow.value());
			if (current_node_slow.empty()) return {};

			current_node_fast = double_walk(current_node_fast.value());
			if (current_node_fast.empty()) return {};

			if (current_node_slow.value() == current_node_fast.value()) break;
		}

		/****************************************************\
		| Now, we have found a cycle. We reconstruct it.     |
		\****************************************************/

		// Note that due to the assumptions on the active graph usage,
		// the found cycle cannot change while we reconstruct it,
		// as no nodes can be removed from the graph until their active edges
		// are "computed" and removed.

		std::vector<QueryCycle::NodeCycleInfo> cycle_nodes;
		bool                                   is_the_initial_node_on_the_cycle = false;

		// current_node_slow is guaranteed to be on the cycle, as it is the meeting point of slow
		// and fast pointers.
		const NodeID cycle_start = current_node_slow.value();

		NodeID current_node = cycle_start;

		while (true) {
			if (current_node == initial_node_id) is_the_initial_node_on_the_cycle = true;

			auto node_data = active_nodes.getCopy(current_node);
			cycle_nodes.push_back(QueryCycle::NodeCycleInfo{
				.node_id = current_node, .node_context_ref = node_data.node_context_ref });

			current_node = node_data.active_edge.value();
			if (current_node == cycle_start) break;
		}

		if (!is_the_initial_node_on_the_cycle) return {};

		return QueryCycle{ .cycle_nodes = std::move(cycle_nodes) };
	}
}
