#pragma once

#include "node_id.hpp"

#include <base/maps.hpp>

#include <ostream>
#include <vector>

namespace query::detail {

	class QueryGraph {
		/**
		 * @brief Color of a node in the graph that is used for cycle detection.
		 */
		enum class Color {
			Visiting,
			Done,
		};

		/**
		 * @brief Data structure that holds information about a node in the graph.
		 */
		struct NodeData final {
			Color               color;
			std::vector<NodeID> dependencies;

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
		 * The actual data of the graph.
		 */
		base::HashMap<NodeID, NodeData> node_data;

		/**
		 * @brief Helper function to print nodes and their dependencies.
		 * @param nodes Vector of NodeIDs to print.
		 * @param out Output stream to print to.
		 */
		void debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const;

	public:
		QueryGraph()                             = default;
		QueryGraph(const QueryGraph&)            = delete;
		QueryGraph(QueryGraph&&)                 = delete;
		QueryGraph& operator=(const QueryGraph&) = delete;
		QueryGraph& operator=(QueryGraph&&)      = delete;

		enum class DependencyStatus { OK, Cycle };

		/**
		 * @brief Marks beginning of new query calculation.
		 * The graph will add a node to a graph or update its data if it already exists.
		 * @note @p called_by is used only for cycle recovery, addDependency has to be always called
		 * explicitly.
		 */
		void setEntry(detail::NodeID node, detail::NodeID called_by);

		/**
		 * @brief Marks exit of a query calculation.
		 */
		void setExit(detail::NodeID node);

		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);

		[[nodiscard]]
		u64 queryStackSize() const {
			return query_stack_size;
		}

		/**
		 * Returns all dependencies of a @p node_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDeps(detail::NodeID node_id) const;

		/**
		 * Returns all dependencies of a @p node_id of type @p dependency_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDepsFiltered(detail::NodeID node_id, QueryID dependency_id) const;

		void debugPrint(std::ostream& out) const;
		void debugPrintForDrawing(std::ostream& out) const;

		/**
		 * @brief Returns all dependencies of a given query call.
		 */
		template<class Query>
		auto getNodeDeps(typename Query::QKey key) const {
			detail::NodeID node_id = makeNodeID(Query::getID(), key);
			return this->getNodeDeps(node_id);
		}

		/**
		 * @brief Returns all dependencies arising from @p dependency_id of a given query call.
		 */
		template<class Query>
		auto getNodeDepsFiltered(typename Query::QKey key, detail::QueryID dependency_id) const {
			detail::NodeID node_id = makeNodeID(Query::getID(), key);
			return this->getNodeDepsFiltered(node_id, dependency_id);
		}

		~QueryGraph() = default;
	};
}
