#pragma once

// #include "node_id.hpp"

#include <concurrent/base/collections/hash_map.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/internal/node_id_id.hpp>
#include <query_framework/internal/node_id_id_super_map.hpp>

#include <memory>

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
	public:
		/**
		 * Helper struct representing a found query cycle.
		 * See cycleCheck() for more details.
		 */
		struct QueryCycle final {
			struct NodeCycleInfo final {
				NodeIDID                          node_id;
				std::shared_ptr<query::Context> node_context_ref;
			};

			std::vector<NodeCycleInfo> cycle_nodes;
		};


	private:
		/**
		 * Helper struct representing data stored for each active node in the graph.
		 */
		struct ActiveData final {
			std::atomic<MaybeNodeIDID> active_edge = MaybeNodeIDID();
			std::shared_ptr<query::Context> node_context_ref = nullptr;

			static_assert(std::atomic<MaybeNodeIDID>::is_always_lock_free, "active_edge should be lock-free for performance reasons");

			ActiveData() = default;
			ActiveData(const ActiveData& other):
				active_edge(other.active_edge.load(std::memory_order_acquire)),
				node_context_ref(other.node_context_ref)
			{ };

			ActiveData(MaybeNodeIDID active_edge, std::shared_ptr<query::Context> node_context_ref):
				active_edge(active_edge), node_context_ref(std::move(node_context_ref)) {}

			// @TODO: #1886 we will also need to store key refs here (in type-erased way),
			// we might want to put in in multiple hash maps, as key operations will be
			// performed less often and will need less strict synchronization.
			// We might want to store Ref<void> – maybe we need custom base type?.
		};

		NodeIDIDSuperMap<ActiveData> maybe_active_nodes;
		std::atomic<u64>                           active_node_count = 0;


	public:
		struct NodeHandle final {
			Ref<ActiveData> node_data_ref;
		};

		/**
		 * Adds a node to the active query graph.
		 * Panics if node is already present.
		 */
		NodeHandle putNode(NodeIDID node_id, std::shared_ptr<query::Context> node_context_ref);

		/**
		 * Removes a node from the active query graph.
		 */
		void removeNode(NodeIDID node_id);

		/**
		 * @return Current size of the active graph.
		 * Note that this can be called concurrently with other operations,
		 * so the result might be immediately outdated.
		 * It should be treated as a good-enough approximation, or in assertions
		 * such as "size() == 0" to check for emptiness.
		 */
		u64 size() const;

		// NodeHandle getNodeHandle(NodeIDID node_id) {
		// 	return active_nodes.getNodeHandle(node_id);
		// }


		/**
		 * Removes active edge of a given node.
		 */
		void removeEdge(NodeIDID node_id);

		void removeEdgeByHandle(NodeHandle handle);

		/**
		 * Sets new active edge of a given node.
		 */
		void setEdge(NodeIDID node_id, NodeIDID edge);

		void setEdgeByHandle(NodeHandle handle, NodeIDID edge);

		/**
		 * Walks the given node, until there is a cycle, or it can't walk no more.
		 * @return QueryCycle if a cycle was found AND the initial node was part of the cycle,
		 *         empty optional otherwise.
		 *
		 * @note This method uses Floyd's Tortoise and Hare algorithm to detect cycles.
		 *       It might seem not necessary, since we only detect cycles that node_id is part of,
		 *       but it is still needed to prevent infinite looping on actual cycles.
		 */
		 [[nodiscard]]
		base::Optional<QueryCycle> cycleCheck(const NodeIDID initial_node_id) ;
	};
}
