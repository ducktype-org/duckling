#include "active_graph.hpp"

namespace query::internal {

	void ActiveGraph::putNode(NodeID node_id, Ref<query::Context> node_context_ref) {
		active_nodes.put(node_id, { .active_edge = base::Optional<NodeID>(), .node_context_ref = node_context_ref });
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
		// This will panic, on setting edge for node that does not exist, this is the expected behavior.
		active_nodes.callOn(node_id, [](Ref<ActiveData> data_ref) {
			CORE_ASSERT(data_ref->active_edge.has_value(), "Removing edge for node that does not have an active edge");
			data_ref->active_edge.reset();
		});
	}

	void ActiveGraph::setEdge(NodeID node_id, NodeID edge) {
		// This will panic, on setting edge for node that does not exist, this is the expected behavior.
		active_nodes.callOn(node_id, [edge](Ref<ActiveData> data_ref) {
			CORE_ASSERT(
				data_ref->active_edge.empty(),
				"Setting edge for node that already has an active edge"
			);
			data_ref->active_edge = edge;
		});
	}

	base::Optional<NodeID> ActiveGraph::walk(NodeID node_id) const {
		auto edge = active_nodes.atMaybeCopy(node_id);
		if (edge.empty()) return {};
		return edge.value().active_edge;
	}

	base::Optional<ActiveGraph::QueryCycle::NodeCycleInfo> ActiveGraph::cycleWalk(NodeID node_id) const {\
		// PR: this is greatly un-performant, change it

		auto node_data = active_nodes.atMaybeCopy(node_id);
		if (node_data.empty()) return {};

		auto node_edge = node_data.value().active_edge;

		if (node_edge.empty()) return {};

		return QueryCycle::NodeCycleInfo{ 
			.node_id = node_edge.value(),
			.node_context_ref = active_nodes.atMaybeCopy(node_edge.value()).value().node_context_ref 
		};
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

		std::vector<QueryCycle::NodeCycleInfo> cycle_nodes;
		bool                is_the_initial_node_on_the_cycle = false;

		NodeID cycle_start = current_node_slow.value();
		cycle_nodes.push_back({ .node_id = cycle_start, .node_context_ref = active_nodes.atMaybeCopy(cycle_start).value().node_context_ref });
		if (cycle_start == node_id) is_the_initial_node_on_the_cycle = true;

		auto walk_data = cycleWalk(cycle_start).value();
		while (walk_data.node_id != cycle_start) {
			cycle_nodes.push_back(walk_data);
			if (walk_data.node_id == node_id) is_the_initial_node_on_the_cycle = true;
			walk_data = cycleWalk(walk_data.node_id).value();
		}

		if (!is_the_initial_node_on_the_cycle) return {};

		return QueryCycle{ .cycle_nodes = std::move(cycle_nodes) };
	}
}
