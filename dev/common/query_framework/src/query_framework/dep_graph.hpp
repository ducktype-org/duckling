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

	struct NodeId {
		QueryID q_id;
		KeyHash hash;
	};

	enum class DependencyStatus {
		OK, Cycle
	};

	namespace dep_graph {

		// this interface is not all that smart:
		// @TODO: make it better
		// @TODO: some pretty printing should be supported
		// @TODO: when cycle is detected "dep_graph" somehow "cycle" unwrap should happen, and all queries in the cycle should produce "CycleError" that will propagate into any query depending from them

		void recalculatingNode(NodeId node);
		DependencyStatus addDependency(NodeId from, NodeId to);
		void calculated(NodeId node);

	}
}
