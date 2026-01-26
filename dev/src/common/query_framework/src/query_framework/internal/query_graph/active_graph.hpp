#include "node_id.hpp"

#include <concurrent/collections/hash_map.hpp>

#include <base/collections/optional.hpp>

namespace query::internal {
	/**
	 * Graph used to represent the set of active nodes and active edges in the query graph.
	 * Its primary purpose is to store the count of currently executing queries and to detect query
	 * cycles.
	 *
	 * @note Operations on this graph are thread safe, and should handle concurrnet cycle detection.
	 *
	 * @note For now, this is naive implementation performance-wise.
	 * In the future, if this will be noticeable, we might want to optimize it to for example only
	 * storing pointers to data in the proper query graph, for always lock-free operations.
	 */
	class ActiveGraph final {
		struct ActiveData final {
			base::Optional<NodeID> active_edge;

			// @TODO PR?: we will also need to store key refs here (in type-erased way),
			// we might want to put in in multiple hash maps, as key operations will be
			// performed less often and will need less strict synchronization.
			// We might want to store Ref<void> – maybe we need custom base type?.
		};

		concurrent::ConHashMap<NodeID, ActiveData> active_nodes;

	public:
		/**
		 * Adds a node to the query graph.
		 * Panics if node is already present.
		 */
		void putNode(NodeID node_id) { active_nodes.put(node_id, {}); }

		/**
		 * Removes active edge of a given node.
		 */
		void removeEdge(NodeID node_id) { active_nodes.update(node_id, {}); }

		/**
		 * Sets new active edge of a given node.
		 */
		void setEdge(NodeID node_id, NodeID edge) { active_nodes.update(node_id, { edge }); }

		/**
		 * Performs the following:
		 * * if node_id is not present in the graph, return empty optional,
		 * * if node_id does not currently have an active edge, return empty optional,
		 * * otherwise return the node the the active edge of provided node points to.
		 */
		base::Optional<NodeID> walk(NodeID node_id) const {
			auto edge = active_nodes.atMaybeCopy(node_id);
			if (edge.empty()) return {};
			return edge.value().active_edge;
		}

		enum CycleCheckResult : bool {
			CycleFound,
			CycleNotFound,
		};

		/**
		 * Walks the given node, until there is a cycle, or it can't walk no more.
		 */
		CycleCheckResult cycleCheck(const NodeID node_id) const {
			auto current_node_slow = node_id;
			auto current_node_fast = node_id;

			while (true) {
				auto next = walk(current_node);

				if (next.empty()) return CycleCheckResult::CycleNotFound;

				if (next.value() == node_id) return CycleCheckResult::CycleFound;

				current_node = next.value();
			}
		}
	};
}
