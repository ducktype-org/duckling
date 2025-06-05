/**
 * @file node_making.hpp
 * @brief Helper functions for generating query `NodeID`.
 */
#pragma once

#include "../../query_hash.hpp"
#include "node_id.hpp"

#include <query_framework/detail/query_data/query_id.hpp>

#include <base/bit256.hpp>

namespace query::detail {

	/**
	 * @brief Helper function for construing NodeID.
	 */
	template<typename KeyType>
	detail::NodeID makeNodeID(QueryID id, const KeyType& key) {
		return { .q_id = id, .hash = { .val = unstableHashKey(key) } };
	}

}
