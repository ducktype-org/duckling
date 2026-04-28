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


	class SipHashLowRoundOnlyU64 final {
	private:
		u64 v0, v1, v2, v3;
		u64 k0, k1;

		u64 total_len = 0;

		u64 buffer = 0;
		u64  buffer_size = 0;

		static constexpr u64 rotl(u64 x, int b) noexcept {
			return (x << b) | (x >> (64 - b));
		}

		constexpr void sip_round() noexcept {
			v0 += v1;
			v1 = rotl(v1, 13);
			v1 ^= v0;
			v0 = rotl(v0, 32);

			v2 += v3;
			v3 = rotl(v3, 16);
			v3 ^= v2;

			v0 += v3;
			v3 = rotl(v3, 21);
			v3 ^= v0;

			v2 += v1;
			v1 = rotl(v1, 17);
			v1 ^= v2;
			v2 = rotl(v2, 32);
		}

		constexpr void compress(u64 m) noexcept {
			v3 ^= m;
			sip_round();
			// sip_round();
			v0 ^= m;
		}

	public:
		using result_type = u64;

		constexpr SipHashLowRoundOnlyU64(u64 key0 = 0, u64 key1 = 0) noexcept
			: k0(key0), k1(key1)
		{
			v0 = 0x736f6d6570736575ULL ^ k0;
			v1 = 0x646f72616e646f6dULL ^ k1;
			v2 = 0x6c7967656e657261ULL ^ k0;
			v3 = 0x7465646279746573ULL ^ k1;
		}

		constexpr void operator()(u64 data) noexcept {
			compress(data);
			total_len += 8;
		}

		[[nodiscard]]
		constexpr result_type finalize() const noexcept {
			SipHashLowRoundOnlyU64 tmp = *this;

			u64 b = tmp.buffer | (static_cast<u64>(tmp.total_len) << 56);

			tmp.v3 ^= b;
			tmp.sip_round();
			// tmp.sip_round();
			tmp.v0 ^= b;

			tmp.v2 ^= 0xff;
			tmp.sip_round();
			tmp.sip_round();
			// tmp.sip_round();
			// tmp.sip_round();

			return tmp.v0 ^ tmp.v1 ^ tmp.v2 ^ tmp.v3;
		}
	};

template<>
struct std::hash<query::internal::NodeID> final {
	std::size_t operator()(const query::internal::NodeID& key) const {
		// auto l = key.q_id;
		// auto r = key.hash.val;

		// // This is questionable
		// return l.asInt() * 9'223'372'036'854'775'783UL + std::hash<base::Bit256>{}(r);

		// slow, but in ensures uniform distribution of hash values, even for similar keys

		// hashing::SHA256 hasher;
		// hashing::SipHash hasher;
		// hashing::addToHash(hasher, key.q_id.asInt());
		// hashing::addToHash(hasher, key.hash.val.data[0]);
		// hashing::addToHash(hasher, key.hash.val.data[1]);
		// hashing::addToHash(hasher, key.hash.val.data[2]);
		// hashing::addToHash(hasher, key.hash.val.data[3]);

		SipHashLowRoundOnlyU64 hasher;
		hasher(key.q_id.asInt());
		hasher(key.hash.val.data[0]);
		hasher(key.hash.val.data[1]);
		hasher(key.hash.val.data[2]);
		hasher(key.hash.val.data[3]);
		
		auto hash_result = hasher.finalize();
		return hash_result;
		// return hash_result.data[0] ^ hash_result.data[1] ^ hash_result.data[2] ^ hash_result.data[3];
	}
};
