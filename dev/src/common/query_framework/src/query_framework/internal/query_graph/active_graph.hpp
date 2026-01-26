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
		std::atomic<u64>                    active_node_count = 0;

	public:
		/**
		 * Adds a node to the query graph.
		 * Panics if node is already present.
		 */
		void putNode(NodeID node_id) {
			active_nodes.put(node_id, {});
			active_node_count++;
		}

		void removeNode(NodeID node_id) {
			auto was_removed = active_nodes.erase(node_id);
			if (was_removed) active_node_count--;
		}

		/**
		 * @return Current size of the active graph.
		 * Note that this can be called concurrently with other operations,
		 * so the result might be immediately outdated.
		 * It should be treated as a good-enough approximation, or in assertions
		 * such as "size() == 0" to check for emptiness.
		 */
		auto size() const -> u64 { return active_node_count.load(); }

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

		enum class CycleCheckResult : bool {
			CycleFound,
			CycleNotFound,
		};

		/**
		 * Walks the given node, until there is a cycle, or it can't walk no more.
		 */
		CycleCheckResult cycleCheck(const NodeID node_id) const {
			
			auto double_walk = [this](NodeID node_id) -> base::Optional<NodeID> {
				auto walk_one = walk(node_id);
				if (walk_one.empty()) return {};
				return walk(walk_one.value());
			};

			base::Optional<NodeID> current_node_slow = node_id;
			base::Optional<NodeID> current_node_fast = node_id;

			while (true) {
				current_node_slow = walk(current_node_slow.value());
				if (current_node_slow.empty()) return CycleCheckResult::CycleNotFound;
				
				current_node_fast = double_walk(current_node_fast.value());
				if (current_node_fast.empty()) return CycleCheckResult::CycleNotFound;

				if (current_node_slow.value() == current_node_fast.value())
					return CycleCheckResult::CycleFound;
			}

			CORE_UNREACHABLE();
		}
	};
}
