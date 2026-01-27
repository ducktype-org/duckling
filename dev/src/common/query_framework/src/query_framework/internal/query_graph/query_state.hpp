#pragma once

#include "active_graph.hpp"
#include "node_id.hpp"
#include "query_graph.hpp"

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>

namespace query::internal {
	/**
	 * @brief Per-query state powering evaluation across the compiler.
	 * \parallel Must be thread-safe as foundational infrastructure; all query categories assume this.
	 */
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
		 * @brief Data structure that holds (non-graph) information about a node in the graph.
		 *
		 * \parallel it is now empty, but is left, as a placeholder for future per-node data such as
		 * computed/in progress.
		 */
		struct NodeData final {};

		/**
		 * @brief Holds data from the previous compilation: the immutable graph and per-node colors.
		 */
		struct PreviousCompilation final {
			/**
			 * The query graph from the previous compilation.
			 * Do not assume that this graph will remain unchanged.
			 * We steal nodes from this graph into the current graph during merging (only green
			 * nodes can be merged) So you have to be careful when using it. Also do not change
			 * nodes from this graph, unless for merging purposes.
			 * @note We assume that this graph is correct and does not contain cycles.
			 */
			QueryGraph                       graph;
			base::HashMap<NodeID, PrevColor> node_colors;

			PreviousCompilation() = delete;

			PreviousCompilation(QueryGraph&& g, base::HashMap<NodeID, PrevColor>&& colors):
				  graph(std::move(g)),
				  node_colors(std::move(colors)) {}

			PreviousCompilation(QueryGraph&& g): graph(std::move(g)), node_colors() {}
		};

	public:
		QueryState()                             = default;
		QueryState(const QueryState&)            = delete;
		QueryState(QueryState&&)                 = delete;
		QueryState& operator=(const QueryState&) = delete;
		QueryState& operator=(QueryState&&)      = delete;

		/***************************\
		| Simple graph interface:   |
		\***************************/

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
		base::Optional<base::CRef<QueryGraph>> getPreviousGraph() const;

		/**
		 * @brief Adds a node to the query graph.
		 *
		 * If the node already exists, resets its data.
		 * @TODO: #1889 in the future, we might want to disallow cache less queries and panic on
		 * adding existing node
		 */
		void addGraphNode(NodeID node_id);

		/*********************************\
		| Active query state interface:   |
		\*********************************/

		Ref<ActiveGraph> getActiveGraph() { return &active_graph; }

		/**
		 * @brief Returns the amount of currently active queries.
		 */
		[[nodiscard]]
		u64 activeQueryCount() const;

		// /**
		//  * @brief Marks beginning of new query calculation.
		//  * The graph will add a node to a graph or update its data if it already exists.
		//  */
		// void setEntry(internal::NodeID node);

		// /**
		//  * @brief Marks exit of a query calculation.
		//  */
		// void setExit(internal::NodeID node);


		/***************************\
		| Incremental interface:    |
		\***************************/


		/**
		 * @brief Sets the color of a node from the previous compilation.
		 * Should only be used by incremental handling logic.
		 */
		void setPrevNodeColor(internal::NodeID node, PrevColor color);

		/**
		 * @brief Returns previous_node_colors map. Used for Tests.
		 * Does not perform any red-green logic, just returns the map as-is.
		 */
		[[nodiscard]]
		base::CRef<base::HashMap<NodeID, PrevColor>> getPreviousNodeColors() const;

		/**
		 * @brief Sets the previous query graph.
		 */
		void setPreviousGraph(QueryGraph&& graph);

		/**
		 * @brief Maps NodeIDs read from a previous graph into IDs valid in the current run by
		 * registering dummy queries for unregistered and unstable IDs and reusing stable ones.
		 * @note This is for internal use in QueryFramework only. It is used to map nodes when
		 * deserializing previous graph in incremental compilation.
		 */
		NodeID remapUnstableOrUnregisteredNodes(NodeID node);

		/**
		 * Performs a red-green sweep starting from the specified node in the current query graph.
		 * This function propagates the red/green markings through the graph to determine which
		 * nodes need to be recomputed.
		 * @return the color of the start_node after the sweep.
		 * @param start_node The starting node for the red-green sweep.
		 */
		PrevColor redGreenSweep(NodeID start_node);

		/**
		 * Merges the previous query graph into the current query graph.
		 * This function updates the current graph with the nodes and edges from the previous graph.
		 * @param start_node The starting node for the merge operation.
		 * @note This function should be called after the red-green sweep to ensure that only the
		 * relevant nodes are merged.
		 * This function will only merge nodes that are not merged yet.
		 * This function assumes that the previous graph is acyclic.
		 * It will panic if a cycle is detected during the merge.
		 * The nodes with unstable hashes will be assigned new QueryIDs to avoid collisions in the
		 * current graph. The new QueryIDs will be a 'dummy' queries. Dummy queries in next
		 * compilation will be unregistered.
		 */
		void mergePreviousGraphIntoCurrentGraph(NodeID start_node);


		/***************************\
		| Serialization interface:  |
		\***************************/


		/**
		 * @brief Builds a reduced adjacency list without mutating the original graph.
		 * @note The returned ReducedGraphData should generally be passed directly to
		 * QueryGraph::serializeReducedGraph without further mutation. This function already
		 * produces the compact graph representation expected by serialization.
		 * @return ReducedGraphData with compacted NodeIDs and adjacency (usize indices) used for
		 * serialization.
		 */
		[[nodiscard]] QueryGraph::ReducedGraphData reduceOptimizeGraph(const QueryGraph& graph
		) const;

	private:
		/***************************\
		| All of the actual state:  |
		\***************************/


		/**
		 * The runtime data of the graph.
		 * \parallel it is now empty, but is left, as a placeholder for future per-node data such as
		 * computed/in progress.
		 * @TODO PR: synchronize code ideas with task pool changes
		 */
		base::HashMap<NodeID, NodeData> node_data;

		/**
		 * The query graph that holds the dependencies and structure of the queries.
		 */
		QueryGraph query_graph;

		/**
		 * The active graph that holds the currently active queries.
		 */
		ActiveGraph active_graph;

		/**
		 * The previous compilation data if any.
		 */
		base::Optional<PreviousCompilation> previous;
	};
}
