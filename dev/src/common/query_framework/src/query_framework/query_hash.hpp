#pragma once

#include <base/bit256.hpp>
#include <base/types/ints.hpp>

namespace query {
	using QueryStableHash = base::Bit256;

	/**
	 * @brief Helper struct to determine the hash type for a given KeyType.
	 * Specializations handle cases where KeyType is u64, bool, or has a queryUnstablePerfectHash
	 * method.
	 */
	template<typename KeyType, bool IsU64OrBool>
	struct UKHashHelper;

	/**
	 * @brief Specialization of UKHashHelper for KeyType that is u64 or bool.
	 * Defines the hash type as u64.
	 */
	template<typename KeyType>
	struct UKHashHelper<KeyType, true> {
		using type = u64;
	};

	/**
	 * @brief Specialization of UKHashHelper for KeyType with queryUnstablePerfectHash.
	 * Defines the hash type as the return type of queryUnstablePerfectHash().
	 */
	template<typename KeyType>
	struct UKHashHelper<KeyType, false> {
		using type = decltype(std::declval<KeyType>().queryUnstablePerfectHash());
	};

	/**
	 * @brief Type alias to determine the hash type for a given KeyType.
	 * If KeyType is u64 or bool, the hash type is u64. Otherwise, it is the return type of
	 * queryUnstablePerfectHash().
	 */
	template<typename KeyType>
	using UKHash = typename UKHashHelper<
		KeyType,
		std::is_same_v<std::remove_cvref_t<KeyType>, u64>
			|| std::is_same_v<std::remove_cvref_t<KeyType>, bool>>::type;

	/**
	 * @brief Gets hash from a key.
	 * As of right now it is assumed that hashKey is collision less (per query).
	 * It has to be ensured by a programmer.
	 * For things like SymID / StrID / ints it is trivial.
	 * For other types it might be necessary to increase hash size to 128 bits
	 * and use "legit hashing algorithm".
	 */
	template<typename KeyType>
	UKHash<KeyType> unstableHashKey(const KeyType& key) {
		if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, u64>)
			return key;
		else if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, bool>)
			return static_cast<u64>(key);
		else
			return key.queryUnstablePerfectHash();
	}

	// Concept to validate queryUnstablePerfectHash signature
	template<typename KeyType>
	concept HasUnstablePerfectHash
		= std::is_same_v<KeyType, bool> || std::is_same_v<KeyType, u64> || requires(KeyType t) {
			  { t.queryUnstablePerfectHash() } -> std::same_as<u64>;
		  } || requires(KeyType t) {
			  { t.queryUnstablePerfectHash() } -> std::same_as<base::Bit256>;
		  };

	// Concept to validate queryStablePerfectHash signature
	template<typename KeyType>
	concept HasStablePerfectHash = requires(KeyType t) {
		{ t.queryStablePerfectHash() } -> std::same_as<QueryStableHash>;
	};
}
