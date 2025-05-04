/**
 * @file node_id.hpp
 * @brief Definition of `NodeID` type, that identifies query node inside dependency graph.
 */
#pragma once

#include "detail/query_id.hpp"

namespace query::detail {

	/**
	 * @brief Type representing hash value for all key-types.
	 */
	struct KeyHash final {
		u64 val;
	};

	/**
	 * @brief Struct representing
	 * dep_graph node of concrete query invocation.
	 */
	struct NodeID final {
		QueryID q_id;
		KeyHash hash;
	};
}
