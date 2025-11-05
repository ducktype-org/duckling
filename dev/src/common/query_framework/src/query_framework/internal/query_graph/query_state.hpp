#pragma once

#include "node_id.hpp"
#include "query_graph.hpp"

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>

namespace query::internal {
	class QueryState final {
	public:
		/**
		 * @brief Color of a node in graph from previous compilation.
		 * Red   - node is outdated
		 * Green - node is up to date
		 */
		enum class PrevColor { Red, Green };

	private:
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
		 * Amount of actively calculating queries.
		 */
		u64 query_stack_size = 0;

		/**
		 * The runtime data of the graph.
		 */
		base::HashMap<NodeID, NodeData> node_data;

		/**
		 * @brief Holds data from the previous compilation: the immutable graph and per-node colors.
		 */
		struct PreviousCompilation final {
			const QueryGraph                 graph;
			base::HashMap<NodeID, PrevColor> node_colors;

			PreviousCompilation() = delete;

			PreviousCompilation(QueryGraph&& g, base::HashMap<NodeID, PrevColor>&& colors):
				  graph(std::move(g)),
				  node_colors(std::move(colors)) {}

			PreviousCompilation(QueryGraph&& g): graph(std::move(g)), node_colors() {}
		};

		/**
		 * The query graph that holds the dependencies and structure of the queries.
		 */
		QueryGraph query_graph;

		/**
		 * The previous compilation data if any.
		 */
		base::Optional<PreviousCompilation> previous;

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
		base::Optional<base::CRef<QueryGraph>> getPreviousGraph() const {
			if (!previous.has_value()) return base::Optional<base::CRef<QueryGraph>>{};
			return &previous.value().graph;
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
		 * Should only be used by incremental handling logic.
		 */
		void setPrevNodeColor(internal::NodeID node, PrevColor color) {
			CORE_ASSERT(
				previous.has_value(), "PreviousCompilation is not set when setting node color"
			);
			previous->node_colors.insert_or_assign(node, color);
		}

		/**
		 * @brief Returns previous_node_colors map. Used for Tests.
		 * Does not perform any red-green logic, just returns the map as-is.
		 */
		[[nodiscard]]
		base::CRef<base::HashMap<NodeID, PrevColor>> getPreviousNodeColors() const {
			CORE_ASSERT(
				previous.has_value(), "PreviousCompilation is not set when accessing node colors"
			);
			return &previous.value().node_colors;
		}

		/**
		 * @brief Sets the previous query graph.
		 */
		void setPreviousGraph(QueryGraph&& graph) {
			CORE_ASSERT(!previous.has_value(), "Previous graph is already set");
			previous.emplace(std::move(graph));
		}
	};
}
