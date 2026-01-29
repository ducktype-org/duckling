#pragma once

#include "node_id.hpp"
#include "query_graph.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/internal/query_metadata/metadata_storage.hpp>

namespace query {
	// Forward declaration
	struct Context;
}

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

			/**
			 * Metadata from previous compilation.
			 * Metadata for green nodes will be moved into current metadata_storage during merge.
			 */
			base::Optional<MetadataStorage> metadata;

			PreviousCompilation() = delete;

			PreviousCompilation(QueryGraph&& g, base::HashMap<NodeID, PrevColor>&& colors):
				  graph(std::move(g)),
				  node_colors(std::move(colors)),
				  metadata() {}

			PreviousCompilation(QueryGraph&& g): graph(std::move(g)), node_colors(), metadata() {}
		};

		/**
		 * The query graph that holds the dependencies and structure of the queries.
		 */
		QueryGraph query_graph;

		/**
		 * The previous compilation data if any.
		 */
		base::Optional<PreviousCompilation> previous;

		/**
		 * @brief Storage for metadata attached to query nodes.
		 */
		MetadataStorage metadata_storage;

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
		base::Optional<base::CRef<QueryGraph>> getPreviousGraph() const;

		/**
		 * @brief Returns the current size of the query stack.
		 */
		[[nodiscard]]
		u64 queryStackSize() const;

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
		 * @brief Sets the previous compilation metadata storage.
		 * Must be called after setPreviousGraph.
		 */
		void setPreviousMetadata(MetadataStorage&& metadata);

		/**
		 * @brief Maps NodeIDs read from a previous graph into IDs valid in the current run by
		 * registering dummy queries for unregistered and unstable IDs and reusing stable ones.
		 * @note This is for internal use in QueryFramework only. It is used to map nodes when
		 * deserializnig previous graph in incremental compilation.
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

		// ========== Metadata API ==========

		/**
		 * @brief Get all metadata of a specific type for a node.
		 *
		 * @tparam MetadataT The metadata type to retrieve (must derive from BaseMetadata)
		 * @param node_id The NodeID to get metadata for
		 * @return std::vector<CRef<MetadataT>> References to all metadata of the given type.
		 *         Returns empty vector if no metadata of this type exists.
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		std::vector<CRef<MetadataT>> getMetadata(NodeID node_id) const {
			return metadata_storage.getMetadata<MetadataT>(node_id);
		}

		/**
		 * @brief Check if a node has any metadata of a specific type.
		 *
		 * @tparam MetadataT The metadata type to check for
		 * @param node_id The NodeID to check
		 * @return true if the node has at least one metadata of this type
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		bool hasMetadata(NodeID node_id) const {
			return metadata_storage.hasMetadata<MetadataT>(node_id);
		}

		/**
		 * @brief Get count of metadata of a specific type for a node.
		 *
		 * @tparam MetadataT The metadata type to count
		 * @param node_id The NodeID to check
		 * @return usize Number of metadata instances of this type
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		usize getMetadataCount(NodeID node_id) const {
			return metadata_storage.getMetadataCount<MetadataT>(node_id);
		}

		/**
		 * @brief Get the metadata storage for direct access.
		 * @note Prefer using the typed getMetadata<T>() method.
		 * @return const reference to the metadata storage.
		 */
		[[nodiscard]]
		const MetadataStorage& getMetadataStorage() const {
			return metadata_storage;
		}

	private:
		friend struct ::query::Context;

		/**
		 * @brief Add metadata to a node. Only accessible via Context.
		 *
		 * @tparam MetadataT The metadata type (must derive from BaseMetadata)
		 * @tparam Args Argument types for constructing the metadata
		 * @param node_id The NodeID to attach metadata to
		 * @param args Arguments forwarded to MetadataT constructor
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, BaseMetadata>
		void addMetadataInternal(NodeID node_id, Args&&... args) {
			metadata_storage.addMetadata<MetadataT>(node_id, std::forward<Args>(args)...);
		}
	};
}
