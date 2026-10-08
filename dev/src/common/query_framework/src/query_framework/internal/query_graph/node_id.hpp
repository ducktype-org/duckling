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

#include <query_framework/internal/query_data/query_id.hpp>  // IWYU pragma: export
#include <ser/base/types.hpp>  // IWYU pragma: keep - the base::Bit256 adapter KeyHash needs
#include <ser/macros.hpp>

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

		/**
		 * @brief `ser` hooks: the query id followed by the key hash.
		 */
		SER_DESCRIBE_MAKE(NodeID, q_id, hash)
	};
}

template<>
struct std::hash<query::internal::NodeID> final {
	std::size_t operator()(const query::internal::NodeID& key) const {
		auto l = key.q_id;
		auto r = key.hash.val;

		// This is questionable
		return l.asInt() * 9'223'372'036'854'775'783UL + std::hash<base::Bit256>{}(r);
	}
};
