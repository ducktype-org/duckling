#pragma once

#include "query_id.hpp"

namespace query {
	
	struct KeyHash {
		u64 val;
	};

	/**
	 * @brief Gets hash from key.
	 * As of right now it is assumed that hashKey is collision less (per query).
	 * It has to be ensured by a programmer.
	 * For things like SymID / StrID / ints it is trivial.
	 * For other types it might be necessary to increase hash size to 128 bits and use "legit hashing algorithm". 
	 */
	template<typename KeyType>
	KeyHash hashKey(const KeyType& key) {
		return { std::hash<KeyType>()(key) };
	}

	struct NodeID {
		QueryID q_id;
		KeyHash hash;
	};

	template<typename KeyType>
	NodeID makeNodeID(QueryID id, const KeyType& key) {
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

		void setEntry(NodeID node);
		DependencyStatus addDependency(NodeID from, NodeID to);
		void setExit(NodeID node);

		void debugPrint();

	}
}
