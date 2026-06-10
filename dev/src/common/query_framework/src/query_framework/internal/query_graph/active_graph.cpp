#include "active_graph.hpp"

#include <utility>

namespace query::internal {

	ActiveGraph::NodeHandle ActiveGraph::putNode(NodeIDID node_id, std::shared_ptr<query::Context> node_context_ref) {
		auto out = active_nodes.put(
			node_id,
			{
				.active_edge      = MaybeNodeIDID(),
				.node_context_ref = std::move(node_context_ref),
			}
		);
		active_node_count++;
		return {out};
	}

	void ActiveGraph::removeNode(NodeIDID node_id) {
		auto was_removed = active_nodes.erase(node_id);

		if (was_removed)
			active_node_count--;
		else
			CORE_PANIC("Removing non-existing node from active graph");
	}

	u64 ActiveGraph::size() const { return active_node_count.load(); }

	void ActiveGraph::removeEdge(NodeIDID node_id) {
		// Note that callOn will panic here, on a node that does not exist, this is the expected
		// behavior.
		active_nodes.at(node_id)->active_edge.store(MaybeNodeIDID(), std::memory_order_release);
	}

	void ActiveGraph::removeEdgeByHandle(NodeHandle handle) {
		handle.node_data_ref->active_edge.store(MaybeNodeIDID(), std::memory_order_release);
		// active_nodes.callOnNodeHandle(handle, [](Ref<ActiveData> data_ref) {
		// 	CORE_ASSERT(
		// 		data_ref->active_edge.has_value(),
		// 		"Removing edge for node that does not have an active edge"
		// 	);
		// 	data_ref->active_edge.reset();
		// });
	}

	void ActiveGraph::setEdge(NodeIDID node_id, NodeIDID edge) {
		// Note that callOn will panic here, on a node that does not exist, this is the expected
		// behavior.
		// active_nodes.callOn(node_id, [edge](Ref<ActiveData> data_ref) {
		// 	CORE_ASSERT(
		// 		data_ref->active_edge.empty(),
		// 		"Setting edge for node that already has an active edge"
		// 	);
		// 	data_ref->active_edge = edge;
		// });

		active_nodes.at(node_id)->active_edge.store(MaybeNodeIDID{edge}, std::memory_order_release);
	}

	void ActiveGraph::setEdgeByHandle(NodeHandle handle, NodeIDID edge) {
		// active_nodes.callOnNodeHandle(handle, [edge](Ref<ActiveData> data_ref) {
		// 	CORE_ASSERT(
		// 		data_ref->active_edge.empty(),
		// 		"Setting edge for node that already has an active edge"
		// 	);
		// 	data_ref->active_edge = edge;
		// });
		handle.node_data_ref->active_edge.store(MaybeNodeIDID{edge}, std::memory_order_release);
	}

	base::Optional<ActiveGraph::QueryCycle> ActiveGraph::cycleCheck(const NodeIDID initial_node_id
	) const {
		/***********************************************************\
		| Cycle detection algorithm: Floyd's Tortoise and Hare.     |
		\***********************************************************/

		// Walks a single edge in the active graph.
		auto walk = [this](NodeIDID node_id) -> base::Optional<NodeIDID> {
			auto edge = active_nodes.atMaybeCopy(node_id);
			if (edge.empty()) return {};
			return edge.value().active_edge.load().asOptional();
		};

		auto double_walk = [walk](NodeIDID walk_zero) -> base::Optional<NodeIDID> {
			auto walk_one = walk(walk_zero);
			if (walk_one.empty()) return {};
			return walk(walk_one.value());
		};

		base::Optional<NodeIDID> current_node_slow = initial_node_id;
		base::Optional<NodeIDID> current_node_fast = initial_node_id;

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

		// Note that IT MIGHT SEEM that due to the assumptions on the active graph usage,
		// the found cycle cannot change while we reconstruct it,
		// as no nodes can be removed from the graph until their active edges
		// are "computed" and removed.
		//
		// This is almost true.
		// There is however a case, where two or more workers enter here concurrently and find the
		// same cycle. Then one of them reconstructs the cycle and return first, and queries on the
		// cycle might continue their execution and remove nodes and edges from the cycle, while the
		// other worker is still reconstructing it.
		//
		// For this reason we must check, while reconstructing the cycle, for any empty edges / non
		// existing nodes, and if we find any, we must return an empty result, as we can reconstruct
		// the cycle. It still keeps the system in a correct state, as the cycle is guaranteed to be
		// correctly reconstructed by at least one worker AND the edge on a cycle can be removed
		// only after the cycle is marked in all Context::is_cyclic_node.

		std::vector<QueryCycle::NodeCycleInfo> cycle_nodes;
		bool                                   is_the_initial_node_on_the_cycle = false;

		// current_node_slow is guaranteed to be on the cycle, as it is the meeting point of slow
		// and fast pointers.
		const NodeIDID cycle_start = current_node_slow.value();

		NodeIDID current_node = cycle_start;

		while (true) {
			if (current_node == initial_node_id) is_the_initial_node_on_the_cycle = true;

			auto node_data = active_nodes.atMaybeCopy(current_node);

			if (node_data.empty()) {
				// This is the case described in the comment above, where we cannot reconstruct the
				// cycle.
				return {};
			}

			cycle_nodes.push_back(QueryCycle::NodeCycleInfo{
				.node_id = current_node, .node_context_ref = node_data.value().node_context_ref });

			auto edge = node_data.value().active_edge.load().asOptional();
			if (edge.empty()) {
				// This is the case described in the comment above, where we cannot reconstruct the
				// cycle.
				return {};
			}
			current_node = edge.value();

			if (current_node == cycle_start) break;
		}

		if (!is_the_initial_node_on_the_cycle) return {};

		return QueryCycle{ .cycle_nodes = std::move(cycle_nodes) };
	}
}
