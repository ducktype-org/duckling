#pragma once

#include "node_id.hpp"
#include "node_making.hpp"

#include <base/collections/maps.hpp>

#include <ostream>
#include <span>
#include <vector>

namespace query::internal {

	class QueryState;

	class QueryGraph final {
		base::HashMap<NodeID, std::vector<NodeID>> node_deps;
		/*
		 * for direct acces to node_deps
		 */
		friend class QueryState;
		/**
		 * @brief Helper function to print nodes and their dependencies.
		 * @param nodes Vector of NodeIDs to print.
		 * @param out Output stream to print to.
		 */
		void debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const;

	public:
		/**
		 * @brief Reduced graph representation used for compact serialization.
		 */
		struct ReducedGraphData final {
			std::vector<NodeID>             nodes;
			std::vector<std::vector<usize>> adjacency;
		};

		QueryGraph()                             = default;
		QueryGraph(const QueryGraph&)            = delete;
		QueryGraph(QueryGraph&&)                 = default;
		QueryGraph& operator=(const QueryGraph&) = delete;
		QueryGraph& operator=(QueryGraph&&)      = delete;

		enum class DependencyStatus { OK, Cycle };

		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		DependencyStatus addDependency(internal::NodeID from, internal::NodeID to);

		/**
		 * Returns all dependencies of a @p node_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDeps(internal::NodeID node_id) const;

		/**
		 * Returns all dependencies of a @p node_id of type @p dependency_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDepsFiltered(internal::NodeID node_id, QueryID dependency_id)
			const;

		/** @brief Returns the immediate dependencies of a @p node_id. */
		[[nodiscard]] const std::vector<NodeID>& getDirectDependencies(const NodeID& node_id) const;

		void debugPrint(std::ostream& out) const;
		void debugPrintForDrawing(std::ostream& out) const;

		/**
		 * @brief Returns all dependencies of a given query call.
		 */
		template<class Query>
		auto getNodeDeps(typename Query::QKey key) const {
			internal::NodeID node_id = makeNodeID<Query>(key);
			return this->getNodeDeps(node_id);
		}

		/**
		 * @brief Returns all dependencies arising from @p dependency_id of a given query call.
		 */
		template<class Query>
		auto getNodeDepsFiltered(typename Query::QKey key, internal::QueryID dependency_id) const {
			internal::NodeID node_id = makeNodeID<Query>(key);
			return this->getNodeDepsFiltered(node_id, dependency_id);
		}

		/**
		 * @brief Serializes the QueryGraph into a vector of bytes.
		 * @note This DOES NOT optimize anything, it just serializes.
		 * @return A vector of bytes representing the serialized QueryGraph.
		 */
		[[nodiscard]] std::vector<byte> serialize() const;

		/**
		 * @brief Serializes an already reduced graph description.
		 * @details The provided mapping must mirror the exact structure we intend to persist, i.e.
		 * each adjacency index references the precomputed NodeID at the same position. This helper
		 * is meant for scenarios where another algorithm (e.g. QueryState::reduceOptimizeGraph) has
		 * already produced a compacted graph representation and we only need to emit bytes without
		 * rebuilding the mapping.
		 */
		[[nodiscard]] static std::vector<byte> serializeReducedGraph(ReducedGraphData reduced_graph);

		/**
		 * @brief Deserializes a QueryGraph from a vector of bytes.
		 * @param data The vector of bytes to deserialize from.
		 * @param node_mapper Optional mapper that can transform NodeIDs read from disk into the
		 *        NodeIDs that should be stored inside the graph. By default it is an identity
		 *        function, but callers can override it to keep the query framework state consistent.
		 * @return A deserialized QueryGraph object.
		 */
		static QueryGraph deserialize(
			std::span<const byte> data, std::function<NodeID(NodeID)> node_mapper = {}
		);

		/**
		 * @brief Compares this QueryGraph with another for equality. For testing purposes.
		 * @param other The other QueryGraph to compare with.
		 * @return True if the graphs are equal, false otherwise.
		 */
		[[nodiscard]] bool compare(const QueryGraph& other) const;

		/**
		 * @brief Checks if a node exists in the graph.
		 * @param node_id The NodeID to check.
		 * @return True if the node exists, false otherwise.
		 */
		[[nodiscard]]
		bool nodeExists(const NodeID& node_id) const {
			return node_deps.contains(node_id);
		}

		/**
		 * @brief Get all Nodes in the graph.
		 * @return A vector of all NodeIDs in the graph.
		 */
		[[nodiscard]] std::vector<NodeID> getAllNodes() const;

		/** @brief Check if a node has any dependencies. */
		[[nodiscard]] bool hasDependencies(const NodeID& node_id) const;

		~QueryGraph() = default;
	};
}
