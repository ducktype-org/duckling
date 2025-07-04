#pragma once

#include "node_id.hpp"
#include "query_graph.hpp"

#include <base/maps.hpp>
#include <base/ref.hpp>

namespace query::internal {
	class QueryState {
		/**
		 * @brief Color of a node in the graph that is used for cycle detection.
		 */
		enum class Color {
			Visiting,
			Done,
		};

		/**
		 * @brief Data structure that holds (non-graph) information about a node in the graph.
		 */
		struct NodeData final {
			Color color;

			/**
			 * @brief NodeID of last node "calling" this query.
			 * Should only hold value when color==Visiting.
			 * Used for cycle recovery.
			 */
			NodeID parent;
		};

		/**
		 * Amount of actively calculating queries.
		 */
		u64 query_stack_size = 0;

		/**
		 * The runtime data of the graph.
		 */
		base::HashMap<NodeID, NodeData> node_data;

		/**
		 * The query graph that holds the dependencies and structure of the queries.
		 */
		QueryGraph query_graph;

	public:
		QueryState()                             = default;
		QueryState(const QueryState&)            = delete;
		QueryState(QueryState&&)                 = delete;
		QueryState& operator=(const QueryState&) = delete;
		QueryState& operator=(QueryState&&)      = delete;

		/**
		 * @brief Returns the mutable query graph.
		 */
		Ref<QueryGraph> getGraphMutable() { return &query_graph; }

		/**
		 * @brief Returns the query graph.
		 */
		[[nodiscard]]
		const QueryGraph& getGraph() const {
			return query_graph;
		}

		/**
		 * @brief Returns the current size of the query stack.
		 */
		[[nodiscard]]
		u64 queryStackSize() const {
			return query_stack_size;
		}

		/**
		 * @brief Marks beginning of new query calculation.
		 * The graph will add a node to a graph or update its data if it already exists.
		 * @note @p called_by is used only for cycle recovery, addDependency has to be always called
		 * explicitly.
		 */
		void setEntry(internal::NodeID node, internal::NodeID called_by);

		/**
		 * @brief Marks exit of a query calculation.
		 */
		void setExit(internal::NodeID node);
	};
}
