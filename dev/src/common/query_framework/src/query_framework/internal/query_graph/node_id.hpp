// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file node_id.hpp
 * @brief Definition of `NodeID` type, that identifies query node inside dependency graph.
 */
#pragma once

#include <base/types/bit256.hpp>
#include <base/types/hash_mix.hpp>

#include <query_framework/internal/query_data/query_id.hpp>  // IWYU pragma: export

#include <functional>

namespace query::internal {

	/**
	 * @brief Type representing hash value for all key-types.
	 */
	struct KeyHash final {
		base::Bit256 val;
	};

	/**
	 * @brief Struct representing
	 * dep_graph node of concrete query invocation.
	 */
	struct NodeID final {
		QueryID q_id;
		KeyHash hash;

		NodeID() = delete;

		NodeID(QueryID q_id, KeyHash hash): q_id(q_id), hash(hash) {}

		constexpr bool operator==(const NodeID& r) const {
			return this->q_id.asInt() == r.q_id.asInt() and this->hash.val == r.hash.val;
		}

		constexpr bool operator<(const NodeID& r) const {
			if (this->q_id.asInt() == r.q_id.asInt()) return this->hash.val < r.hash.val;
			return this->q_id.asInt() < r.q_id.asInt();
		}
	};
}

template<>
struct std::hash<query::internal::NodeID> final {
	std::size_t operator()(const query::internal::NodeID& key) const {
		// Mix the query id into the key hash. The previous `q_id * (2^63 - 25) + key_hash` shifted
		// consecutive query ids by only -25, so small key hashes of different queries collided.
		return base::hashCombine64(
			std::hash<base::Bit256>{}(key.hash.val), static_cast<u64>(key.q_id.asInt())
		);
	}
};
