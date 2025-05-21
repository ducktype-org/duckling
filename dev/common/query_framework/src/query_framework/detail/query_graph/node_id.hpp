/**
 * @file node_id.hpp
 * @brief Definition of `NodeID` type, that identifies query node inside dependency graph.
 */
#pragma once

#include <query_framework/detail/query_data/query_id.hpp>  // IWYU pragma: export

namespace query::detail {

	/**
	 * @brief Type representing hash value for all key-types.
	 */
	struct key_hash final {
		u64 val;
	};

	/**
	 * @brief Struct representing
	 * dep_graph node of concrete query invocation.
	 */
	struct NodeID final {
		QueryID q_id;
		key_hash hash;

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
struct std::hash<query::detail::NodeID> final {
	std::size_t operator()(const query::detail::NodeID& key) const {
		auto l = key.q_id;
		auto r = key.hash.val;

		// this is questionable:
		return l.asInt() * 9'223'372'036'854'775'783UL + r;
	}
};
