#pragma once

#include "query_id.hpp"

namespace query::detail {

	/**
	 * @brief Type representing hash value for all key-types.
	 */
	struct KeyHash {
		u64 val;
		constexpr bool operator==(const KeyHash& oth) const = default;
	};

	/**
	 * @brief Struct representing
	 * dep_graph node of concrete query invocation.
	 */
	struct NodeID {
		QueryID q_id;
		KeyHash hash;
		constexpr bool operator==(const NodeID& oth) const = default;
	};

}
