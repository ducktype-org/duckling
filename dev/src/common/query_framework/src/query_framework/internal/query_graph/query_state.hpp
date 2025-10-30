#pragma once

#include "node_id.hpp"
#include "query_graph.hpp"

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>

namespace query::internal {
	class QueryState final {
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

			NodeData() = delete;

			NodeData(Color color, NodeID parent): color(color), parent(parent) {}
		};
		/**
		 * @brief Color of a node in graph from previous compilation.
		 */
		enum class PrevColor { Red, Green };

		/**
		 * Amount of actively calculating queries.
		 */
		u64 query_stack_size = 0;

		/**
		 * The runtime data of the graph.
		 */
		base::HashMap<NodeID, NodeData> node_data;

		/**
		 * The colors of nodes from previous compilation.
		 * Green - node is up to date
		 * Red   - node is outdated
		 */
		base::HashMap<NodeID, PrevColor> previous_node_colors;

		/**
		 * The query graph that holds the dependencies and structure of the queries.
		 */
		QueryGraph query_graph;

		/**
		 * The immutable graph that hold the state from previous compilation.
		 * This is used for incremental compilation.
		 */
		base::Optional<QueryGraph> previous;

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
		 * @brief Returns the graph from previous compilation.
		 */
		[[nodiscard]]
		const QueryGraph& getPreviousGraph() const {
			CORE_ASSERT(previous.has_value(), "Previous graph is not set");
			return *previous;
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

		/**
		 * @brief Sets the color of a node from the previous compilation.
		 */
		void setPrevNodeColor(internal::NodeID node, PrevColor color) {
			previous_node_colors.insert_or_assign(node, color);
		}

		/**
		 * @brief Sets the previous query graph.
		 */
		void setPreviousGraph(QueryGraph&& graph) {
			CORE_ASSERT(!previous.has_value(), "Previous graph is already set");
			// Store the previous graph and mark all its nodes as Red (outdated)
			previous.emplace(std::move(graph));
			// previous is friend of QueryGraph so we can access node_deps directly
			for (const auto& [node, _deps]: previous->node_deps)
				previous_node_colors.insert_or_assign(node, PrevColor::Red);
		}
	};
}
