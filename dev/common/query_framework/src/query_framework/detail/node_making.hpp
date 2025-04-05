/**
 * @file node_making.hpp
 * @brief Helper functions for generating query `NodeID`.
 */
#pragma once

#include "node_id.hpp"
#include "query_id.hpp"

#include <base/perfect_hash.hpp>

namespace query::detail {

	/**
	 * @brief Gets hash from a key.
	 * As of right now it is assumed that hashKey is collision less (per query).
	 * It has to be ensured by a programmer.
	 * For things like SymID / StrID / ints it is trivial.
	 * For other types it might be necessary to increase hash size to 128 bits
	 * and use "legit hashing algorithm".
	 */
	template<typename KeyType>
	detail::KeyHash hashKey(const KeyType& key) {
		return { base::perfectHash<std::remove_cvref_t<KeyType>>(key) };
	}

	/**
	 * @brief Helper function for construing NodeID.
	 */
	template<typename KeyType>
	detail::NodeID makeNodeID(QueryID id, const KeyType& key) {
		return { id, hashKey(key) };
	}

}
