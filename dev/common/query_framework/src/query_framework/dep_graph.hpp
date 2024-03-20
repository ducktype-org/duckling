#pragma once

#include "query_id.hpp"

namespace query {
	
	struct KeyHash {
		u64 val;
	};

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

	}
}
