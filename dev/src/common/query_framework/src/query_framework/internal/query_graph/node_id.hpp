/**
 * @file node_id.hpp
 * @brief Definition of `NodeID` type, that identifies query node inside dependency graph.
 */
#pragma once

#include <base/types/bit256.hpp>

#include <query_framework/internal/query_data/query_id.hpp>  // IWYU pragma: export

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
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
		// auto l = key.q_id;
		// auto r = key.hash.val;

		// // This is questionable
		// return l.asInt() * 9'223'372'036'854'775'783UL + std::hash<base::Bit256>{}(r);

		// slow, but in ensures uniform distribution of hash values, even for similar keys

		// hashing::SHA256 hasher;
		hashing::SipHash hasher;
		hashing::addToHash(hasher, key.q_id.asInt());
		hashing::addToHash(hasher, key.hash.val.data[0]);
		hashing::addToHash(hasher, key.hash.val.data[1]);
		hashing::addToHash(hasher, key.hash.val.data[2]);
		hashing::addToHash(hasher, key.hash.val.data[3]);

		auto hash_result = hasher.finalize();
		return hash_result;
		// return hash_result.data[0] ^ hash_result.data[1] ^ hash_result.data[2] ^ hash_result.data[3];
	}
};
