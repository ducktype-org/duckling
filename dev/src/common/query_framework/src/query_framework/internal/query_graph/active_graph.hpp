#include "node_id.hpp"

#include <concurrent/collections/hash_map.hpp>

#include <base/collections/optional.hpp>

namespace query::internal {
	/**
	 * Graph used to represent the set of active nodes and active edges in the query graph.
	 * Its primary purpose is to store the count of currently executing queries and to detect query
	 * cycles.
	 *
	 * @note Operations on this graph are thread safe, and can handle concurrent cycle detection.
	 *
	 * @note For now, this is a naive implementation performance-wise.
	 * In the future, if this will be noticeable, we might want to optimize it to for example only
	 * storing pointers to data in the proper query graph, for always lock-free operations.
	 */
	class ActiveGraph final {
		struct ActiveData final {
			base::Optional<NodeID> active_edge;

			// @TODO: #1886 we will also need to store key refs here (in type-erased way),
			// we might want to put in in multiple hash maps, as key operations will be
			// performed less often and will need less strict synchronization.
			// We might want to store Ref<void> – maybe we need custom base type?.
		};

		concurrent::ConHashMap<NodeID, ActiveData> active_nodes;
		std::atomic<u64>                           active_node_count = 0;

	public:
		/**
		 * Adds a node to the active query graph.
		 * Panics if node is already present.
		 */
		void putNode(NodeID node_id) {
			active_nodes.put(node_id, {});
			active_node_count++;
		}

		/**
		 * Removes a node from the active query graph.
		 */
		void removeNode(NodeID node_id) {
			auto was_removed = active_nodes.erase(node_id);

			if (was_removed) active_node_count--;
			else CORE_PANIC("Removing non-existing node from active graph");
		}

		/**
		 * @return Current size of the active graph.
		 * Note that this can be called concurrently with other operations,
		 * so the result might be immediately outdated.
		 * It should be treated as a good-enough approximation, or in assertions
		 * such as "size() == 0" to check for emptiness.
		 */
		u64 size() const { return active_node_count.load(); }

		/**
		 * Removes active edge of a given node.
		 */
		void removeEdge(NodeID node_id) { active_nodes.update(node_id, {}); }

		/**
		 * Sets new active edge of a given node.
		 */
		void setEdge(NodeID node_id, NodeID edge) {
			// Note that there might be some concurrent operations
			// between following assertion and update,
			// but the assertion must always pass anyway (when the active graph is used correctly).
			CORE_ASSERT(
				active_nodes.atMaybeCopy(node_id).value().active_edge.empty(),
				"Setting edge for node that already has an active edge"
			);
			active_nodes.update(node_id, { edge });
		}

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

		/**
		 * Helper struct representing a found query cycle.
		 * See cycleCheck() for more details.
		 */
		struct QueryCycle final {
			std::vector<NodeID> cycle_nodes;
		};

		/**
		 * Walks the given node, until there is a cycle, or it can't walk no more.
		 * @return QueryCycle if a cycle was found AND the initial node was part of the cycle,
		 *         empty optional otherwise.
		 *
		 * @note This method uses Floyd's Tortoise and Hare algorithm to detect cycles.
		 *       It might seem not necessary, since we only detect cycles that node_id is part of,
		 *       but it is still needed to prevent infinite looping on actual cycles.
		 */
		base::Optional<QueryCycle> cycleCheck(const NodeID node_id) const {
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
	};
}
