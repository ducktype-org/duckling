/**
 * @file node_making.hpp
 * @brief Helper functions for generating query `NodeID`.
 */
#pragma once

#include "node_id.hpp"

#include <query_framework/detail/query_data/query_id.hpp>

#include <base/bit256.hpp>

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
	u64 hashKey(const KeyType& key) {
		if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, u64>)
			return key;
		else if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, bool>)
			return static_cast<u64>(key);
		else
			return key.queryUnstablePerfectHash();
	}

	/**
	 * @brief Helper function for construing NodeID.
	 */
	template<typename KeyType>
	detail::NodeID makeNodeID(QueryID id, const KeyType& key) {
		return { id, hashKey(key) };
	}

}
