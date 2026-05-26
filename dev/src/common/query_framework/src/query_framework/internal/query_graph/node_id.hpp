/**
 * @file node_id.hpp
 * @brief Definition of `NodeID` type, that identifies query node inside dependency graph.
 */
#pragma once

#include <base/types/bit256.hpp>

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



	class SipHashLowRoundOnlyU64 final {
	private:
		u64 v0, v1, v2, v3;
		u64 k0, k1;

		u64 total_len = 0;

		u64 buffer = 0;

		static constexpr u64 rotl(u64 x, int b) noexcept { return (x << b) | (x >> (64 - b)); }

		constexpr void sipRound() noexcept {
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


	public:
		using result_type = u64;

		constexpr SipHashLowRoundOnlyU64(u64 key0 = 0, u64 key1 = 0) noexcept: k0(key0), k1(key1) {
			v0 = 0x73'6f'6d'65'70'73'65'75ULL ^ k0;
			v1 = 0x64'6f'72'61'6e'64'6f'6dULL ^ k1;
			v2 = 0x6c'79'67'65'6e'65'72'61ULL ^ k0;
			v3 = 0x74'65'64'62'79'74'65'73ULL ^ k1;
		}

		constexpr void operator()(u64 data) noexcept {
			v3 ^= data;
			sipRound();
			// sip_round();
			v0 ^= data;
		}

		[[nodiscard]]
		constexpr result_type finalize() const noexcept {
			SipHashLowRoundOnlyU64 tmp = *this;

			u64 b = tmp.buffer | (static_cast<u64>(tmp.total_len) << 56);

			tmp.v3 ^= b;
			tmp.sipRound();
			// tmp.sip_round();
			tmp.v0 ^= b;

			tmp.v2 ^= 0xff;
			tmp.sipRound();
			tmp.sipRound();
			// tmp.sip_round();
			// tmp.sip_round();

			return tmp.v0 ^ tmp.v1 ^ tmp.v2 ^ tmp.v3;
		}
	};


	struct XXHash64 final {
		uint64_t state = PRIME5;
		uint64_t total_len = 0;

		static constexpr uint64_t PRIME1 = 11400714785074694791ULL;
		static constexpr uint64_t PRIME2 = 14029467366897019727ULL;
		static constexpr uint64_t PRIME3 =  1609587929392839161ULL;
		static constexpr uint64_t PRIME4 =  9650029242287828579ULL;
		static constexpr uint64_t PRIME5 =  2870177450012600261ULL;

		static uint64_t rotl(uint64_t x, int r) {
			return (x << r) | (x >> (64 - r));
		}

		// Add one uint64_t value to the hash
		void add(uint64_t value) {
			total_len += 8;

			uint64_t k = value;
			k *= PRIME2;
			k = rotl(k, 31);
			k *= PRIME1;

			state ^= k;
			state = rotl(state, 27) * PRIME1 + PRIME4;
		}

		// Finalize and return the hash
		[[nodiscard]]
		uint64_t finalize() const {
			uint64_t h = state + total_len;

			h ^= h >> 33;
			h *= PRIME2;
			h ^= h >> 29;
			h *= PRIME3;
			h ^= h >> 32;

			return h;
		}
	};
}

template<>
struct std::hash<query::internal::NodeID> final {
	std::size_t operator()(const query::internal::NodeID& key) const {
		query::internal::XXHash64 hasher;
		hasher.add(key.q_id.asInt());
		hasher.add(key.hash.val.data[0]);
		hasher.add(key.hash.val.data[1]);
		hasher.add(key.hash.val.data[2]);
		hasher.add(key.hash.val.data[3]);
		return hasher.finalize();
		
		// auto l = key.q_id;
		// auto r = key.hash.val;

		// // This is questionable
		// return l.asInt() * 9'223'372'036'854'775'783UL + std::hash<base::Bit256>{}(r);
	}
};
