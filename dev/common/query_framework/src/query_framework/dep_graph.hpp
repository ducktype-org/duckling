#pragma once

#include "query_id.hpp"
#include "node_id.hpp"

namespace query::detail {
	
	/**
	 * @brief Gets hash from key.
	 * As of right now it is assumed that hashKey is collision less (per query).
	 * It has to be ensured by a programmer.
	 * For things like SymID / StrID / ints it is trivial.
	 * For other types it might be necessary to increase hash size to 128 bits and use "legit hashing algorithm". 
	 */
	template<typename KeyType>
	detail::KeyHash hashKey(const KeyType& key) {
		return { std::hash<KeyType>()(key) };
	}


	/**
	 * @brief Helper function for construing NodeID.
	 */
	template<typename KeyType>
	detail::NodeID makeNodeID(QueryID id, const KeyType& key) {
		return { id, hashKey(key) };	
	}

	enum class DependencyStatus {
		OK, Cycle
	};

	namespace dep_graph {

		// this interface is not all that smart:
		// @TODO: make it better
		// @TODO: some pretty printing should be supported
		// @TODO: when cycle is detected "dep_graph" somehow "cycle" unwrap should happen, and all queries in the cycle should produce "CycleError" that will propagate into any query depending from them

		void setEntry(detail::NodeID node, detail::NodeID from);
		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);
		void setExit(detail::NodeID node);

		void debugPrint();

	}
}

namespace query {
	inline void debugPrintDependencyGraph() { detail::dep_graph::debugPrint(); }
}
