#pragma once

#include <base/bit256.hpp>
#include <base/ints.hpp>

namespace query {
	using QueryUnstableHash = u64;
	using QueryStableHash   = base::Bit256;

	/**
	 * @brief Gets hash from a key.
	 * As of right now it is assumed that hashKey is collision less (per query).
	 * It has to be ensured by a programmer.
	 * For things like SymID / StrID / ints it is trivial.
	 * For other types it might be necessary to increase hash size to 128 bits
	 * and use "legit hashing algorithm".
	 */
	template<typename KeyType>
	QueryUnstableHash unstableHashKey(const KeyType& key) {
		if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, u64>)
			return key;
		else if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, bool>)
			return static_cast<QueryUnstableHash>(key);
		else
			return key.queryUnstablePerfectHash();
	}

	// Concept to validate queryUnstablePerfectHash signature
	template<typename KeyType>
	concept HasUnstablePerfectHash
		= std::is_same_v<KeyType, bool> || std::is_same_v<KeyType, u64> || requires(KeyType t) {
			  { t.queryUnstablePerfectHash() } -> std::same_as<QueryUnstableHash>;
		  };

	// Concept to validate queryStablePerfectHash signature
	template<typename KeyType>
	concept HasStablePerfectHash = requires(KeyType t) {
		{ t.queryStablePerfectHash() } -> std::same_as<QueryStableHash>;
	};

	template<class KeyType>
	struct queryUnstableHashFunctor final {
		std::size_t operator()(const KeyType& key) const { return unstableHashKey(key); }
	};
}
