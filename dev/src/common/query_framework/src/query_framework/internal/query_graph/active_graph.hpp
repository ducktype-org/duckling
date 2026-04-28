#pragma once

#include "node_id.hpp"

#include <concurrent/base/collections/hash_map.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context_fd.hpp>

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
				NodeID              node_id;
				Ref<query::Context> node_context_ref;
			};

			std::vector<NodeCycleInfo> cycle_nodes;
		};


	private:
		struct ActiveData final {
			base::Optional<NodeID> active_edge;

			Ref<query::Context> node_context_ref;

			// @TODO: #1886 we will also need to store key refs here (in type-erased way),
			// we might want to put in in multiple hash maps, as key operations will be
			// performed less often and will need less strict synchronization.
			// We might want to store Ref<void> – maybe we need custom base type?.
		};

		concurrent::ConHashMap<NodeID, ActiveData> active_nodes;
		std::atomic<u64>                           active_node_count = 0;

		/**
		 * Performs the following:
		 * * if node_id is not present in the graph, return empty optional,
		 * * if node_id does not currently have an active edge, return empty optional,
		 * * otherwise return the node that the active edge of provided node points to.
		 // TODO PR: refactor it, we should have one function
		 */
		base::Optional<NodeID> walk(NodeID node_id) const;

		/**
		 * Same as walk, but return full info needed to construct a QueryCycle in case of cycle
		 * detection.
		 */
		base::Optional<QueryCycle::NodeCycleInfo> cycleWalk(NodeID node_id) const;

	public:
		/**
		 * Adds a node to the active query graph.
		 * Panics if node is already present.
		 */
		void putNode(NodeID node_id, Ref<query::Context> node_context_ref);

		/**
		 * Removes a node from the active query graph.
		 */
		void removeNode(NodeID node_id);

		/**
		 * @return Current size of the active graph.
		 * Note that this can be called concurrently with other operations,
		 * so the result might be immediately outdated.
		 * It should be treated as a good-enough approximation, or in assertions
		 * such as "size() == 0" to check for emptiness.
		 */
		u64 size() const;

		/**
		 * Removes active edge of a given node.
		 */
		void removeEdge(NodeID node_id);

		/**
		 * Sets new active edge of a given node.
		 */
		void setEdge(NodeID node_id, NodeID edge);

		/**
		 * Walks the given node, until there is a cycle, or it can't walk no more.
		 * @return QueryCycle if a cycle was found AND the initial node was part of the cycle,
		 *         empty optional otherwise.
		 *
		 * @note This method uses Floyd's Tortoise and Hare algorithm to detect cycles.
		 *       It might seem not necessary, since we only detect cycles that node_id is part of,
		 *       but it is still needed to prevent infinite looping on actual cycles.
		 */
		base::Optional<QueryCycle> cycleCheck(const NodeID node_id) const;
	};
}
