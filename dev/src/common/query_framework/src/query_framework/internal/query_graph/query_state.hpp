#pragma once

#include "active_graph.hpp"
#include "node_id.hpp"
#include "query_graph.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <diagnostic_interactive/logger_fwd.hpp>
#include <diagnostic_interactive/message_fwd.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/internal/query_metadata/metadata_storage.hpp>
#include <query_framework/internal/task_pool/task_pool.hpp>

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
			QueryGraph graph;

			/**
			 * Colors of nodes from the previous compilation.
			 * Red   - node is outdated
			 * Green - node is up to date and can be merged into the current graph without
			 * recomputation
			 * @note This map should only be written to during the red-green sweep
			 */
			base::Box<concurrent::ConHashMap<NodeID, PrevColor>> node_colors;

			/**
			 * Metadata from previous compilation.
			 * Metadata for green nodes will be moved into current metadata_storage during merge.
			 * \parallel #29 Make metadata concurrent
			 */
			base::Optional<MetadataStorage> metadata;

			PreviousCompilation() = delete;

			PreviousCompilation(
				QueryGraph&& g, base::Box<concurrent::ConHashMap<NodeID, PrevColor>>&& colors
			):
				  graph(std::move(g)),
				  node_colors(std::move(colors)),
				  metadata() {}

			PreviousCompilation(QueryGraph&& g):
				  graph(std::move(g)),
				  node_colors(base::makeBox<concurrent::ConHashMap<NodeID, PrevColor>>()),
				  metadata() {}
		};

	public:
		QueryState()                             = default;
		QueryState(const QueryState&)            = delete;
		QueryState(QueryState&&)                 = delete;
		QueryState& operator=(const QueryState&) = delete;
		QueryState& operator=(QueryState&&)      = delete;

		/***************************\
		| Query graph interface:    |
		\***************************/

		/**
		 * @brief Returns read-only reference to the query graph.
		 */
		[[nodiscard]]
		const QueryGraph& getGraph() const;

		/**
		 * @brief Returns mutable reference to the query graph.
		 */
		[[nodiscard]]
		QueryGraph& getGraphMutable();

		/**
		 * @brief Returns read-only reference to the graph from previous compilation.
		 */
		[[nodiscard]]
		base::Optional<base::CRef<QueryGraph>> getPreviousGraph() const;

		/**
		 * @brief Adds a node to the query graph.
		 * panics if the node already exists.
		 */
		void addGraphNode(NodeID node_id);

		/**
		 * @brief Adds a node to the query graph that represents a side input query.
		 * This is needed because SideSinput queries have no cache and can be added multiple times.
		 */
		void addSideInputNode(NodeID node_id);


		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		void addDependency(NodeID from, NodeID to);


		/*********************************\
		| Active query state interface:   |
		\*********************************/

		/**
		 *  Return mutable reference to the active graph.
		 *  @note Note that all active graph operations are thread safe.
		 */
		Ref<ActiveGraph> getActiveGraph() noexcept;

		/**
		 * @brief Returns the amount of currently active queries.
		 */
		[[nodiscard]]
		u64 activeQueryCount() const;

		/************************\
		| Task pool interface:   |
		\************************/

		/**
		 * Returns singleton task pool used for handling execution of queries.
		 * @TODO: #2038 change this to a getter of query state member.
		 */
		Ref<TaskPool> getTaskPool() const;

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
		base::CRef<concurrent::ConHashMap<NodeID, PrevColor>> getPreviousNodeColors() const;

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

		/***************************\
		|    Metadata interface:    |
		\***************************/

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
		 * @note This function is used for tests
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
		base::CRef<MetadataStorage> getMetadataStorage() const;

		/**
		 * @brief Get the previous compilation metadata storage.
		 * @return Optional reference to the previous metadata storage, empty if not set.
		 */
		[[nodiscard]]
		base::Optional<base::CRef<MetadataStorage>> getPreviousMetadataStorage() const;

		[[nodiscard]]
		Ref<MetadataStorage> getMetadataStorageMutable();

		/*******************************\
		|    Diagnostics interface:    |
		\******************************/

		/**
		 * @brief Logs a diagnostic message for a specific node.
		 * It creates a logger for the node if it doesn't exist and logs the message to it.
		 */
		void logDiagnosticForNode(NodeID node_id, Box<dia_int::MessageBase> diagnostic);

		/**
		 * @brief Clears all diagnostics for a specific node.
		 */
		void clearDiagnosticForNode(NodeID node_id);

		/**
		 * @brief Gets a diagnostic logger for a specific node, if it exists.
		 * Used for the tests.
		 *
		 * Not thread safe.
		 */
		base::Optional<CRef<dia_int::Logger>> getDiagnosticForNode(NodeID node_id) const;

		/**
		 * @brief Get the entire map of diagnostic loggers for direct access.
		 */
		CRef<concurrent::ConHashMap<NodeID, Box<dia_int::Logger>>> getDiagnosticLoggers() const;

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
			// Check that the query has preserve_in_graph = true
			CORE_ASSERT(
				node_id.q_id.getData().tags.preserve_in_graph,
				"Cannot add metadata to query without preserve_in_graph = true. "
				"Query: "
					+ std::string(node_id.q_id.getData().name)
			);

			metadata_storage.addMetadata<MetadataT>(node_id, std::forward<Args>(args)...);
		}

		/**
		 * @brief Add metadata to a node only if no metadata of this type exists.
		 *
		 * @tparam MetadataT The metadata type (must derive from BaseMetadata)
		 * @tparam Args Argument types for constructing the metadata
		 * @param node_id The NodeID to attach metadata to
		 * @param args Arguments forwarded to MetadataT constructor
		 * @return true if metadata was added, false if it already exists
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, BaseMetadata>
		bool addMetadataIfNotExistsInternal(NodeID node_id, Args&&... args) {
			// Check that the query has preserve_in_graph = true
			CORE_ASSERT(
				node_id.q_id.getData().tags.preserve_in_graph,
				"Cannot add metadata to query without preserve_in_graph = true. "
				"Query: "
					+ std::string(node_id.q_id.getData().name)
			);
			return metadata_storage.addMetadataIfNotExists<MetadataT>(
				node_id, std::forward<Args>(args)...
			);
		}

		/***************************\
		| All of the actual state:  |
		\***************************/

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

		/**
		 * @brief Storage for metadata attached to query nodes.
		 */
		MetadataStorage metadata_storage;


		/**
		 * @brief Storage for the diagnostic loggers for each noe.
		 */
		concurrent::ConHashMap<NodeID, Box<dia_int::Logger>> diagnostic_loggers;
	};
}
