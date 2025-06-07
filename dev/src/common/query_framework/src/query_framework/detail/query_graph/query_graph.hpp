#pragma once

#include "node_id.hpp"

#include <base/maps.hpp>

#include <ostream>
#include <vector>

namespace query::detail {

	class QueryState;

	class QueryGraph {
		base::HashMap<NodeID, std::vector<NodeID>> node_deps;

		/**
		 * @brief Helper function to print nodes and their dependencies.
		 * @param nodes Vector of NodeIDs to print.
		 * @param out Output stream to print to.
		 */
		void debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const;

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
		QueryGraph()                             = default;
		QueryGraph(const QueryGraph&)            = delete;
		QueryGraph(QueryGraph&&)                 = delete;
		QueryGraph& operator=(const QueryGraph&) = delete;
		QueryGraph& operator=(QueryGraph&&)      = delete;

		enum class DependencyStatus { OK, Cycle };

		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);

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
