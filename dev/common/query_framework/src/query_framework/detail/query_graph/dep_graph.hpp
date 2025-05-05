/**
 * @file dep_graph.hpp
 * @brief Implementation of Dependency Graph. Dependency Graph is a data structure used to track
 * dependencies of queries and detect cyclic query calls.
 */
#pragma once

#include "node_id.hpp"

#include <ostream>
#include <vector>

namespace query::detail {

	// @OPT: pick good type size here
	enum class DependencyStatus { OK, Cycle };

	namespace dep_graph {

		// this interface is not all that smart:
		// @TODO: make it better
		// @TODO: some pretty printing should be supported
		// @TODO: when cycle is detected "dep_graph" somehow "cycle" unwrap should happen, and all
		// queries in the cycle should produce "CycleError" that will propagate into any query
		// depending from them

		/**
		 * @brief Returns size of current query stack size
		 */
		u64 queryStackSize();

		/**
		 * @brief Marks beginning of new query
		 */
		void setEntry(detail::NodeID node, detail::NodeID from);

		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);

		/**
		 * @brief Marks exit of a query
		 */
		void setExit(detail::NodeID node);

		void debugPrint(std::ostream& out);
		void debugPrintForDrawing(std::ostream& out);

		/**
		 * Returns all dependencies of a @p node_id.
		 */
		std::vector<NodeID> getNodeDeps(detail::NodeID node_id);

		/**
		 * Returns all dependencies of a @p node_id of type @p dependency_id.
		 */
		std::vector<NodeID> getNodeDepsFilterred(detail::NodeID node_id, QueryID dependency_id);
	}
}

namespace query {
	inline void debugPrintDependencyGraph(std::ostream& out) { detail::dep_graph::debugPrint(out); }

	inline void debugPrintDependencyGraphForDrawing(std::ostream& out) {
		detail::dep_graph::debugPrintForDrawing(out);
	}

	/**
	 * @brief Returns all dependencies of a given query call.
	 */
	template<class Query>
	auto getNodeDeps(typename Query::QKey key) {
		detail::NodeID node_id = makeNodeID(Query::getID(), key);
		return detail::dep_graph::getNodeDeps(node_id);
	}

	/**
	 * @brief Returns all dependencies arising from @p dependency_id of a given query call.
	 */
	template<class Query>
	auto getNodeDepsFiltered(typename Query::QKey key, detail::QueryID dependency_id) {
		detail::NodeID node_id = makeNodeID(Query::getID(), key);
		return detail::dep_graph::getNodeDepsFilterred(node_id, dependency_id);
	}
}
