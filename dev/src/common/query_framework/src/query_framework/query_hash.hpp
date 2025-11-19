/**
 * Utilities for key hashes.
 */

#pragma once

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <type_traits>
#include <utility>

namespace query {
	using QueryStableHash = base::Bit256;

	/**
	 * Concept that checks if a key provides queryUnstablePerfectHash method with valid signature.
	 */
	template<typename KeyType>
	concept HasUnstablePerfectHash = requires(KeyType t) {
		{ t.queryUnstablePerfectHash() } -> std::same_as<u64>;
	} || requires(KeyType t) {
		{ t.queryUnstablePerfectHash() } -> std::same_as<base::Bit256>;
	};

	/**
	 * Concept that checks if a key provides queryStablePerfectHash method with valid signature.
	 */
	template<typename KeyType>
	concept HasStablePerfectHash = requires(KeyType t) {
		{ t.queryStablePerfectHash() } -> std::same_as<QueryStableHash>;
	};

	/**
	 * @brief Type alias for the perfect stable hash type of a given key type.
	 */
	template<typename KeyType>
	using KHashStable = QueryStableHash;

	/**
	 * @brief Type alias for the perfect unstable hash type of a given key type.
	 */
	template<typename KeyType>
	using KHashUnstable
		= decltype(std::declval<std::remove_cvref_t<KeyType>>().queryStablePerfectHash());

	/**
	 * @brief Gets "perfect" hash from a key.
	 */
	template<bool use_stable_hash, typename KeyType>
	auto perfectHashKey(const KeyType& key) {
		if constexpr (use_stable_hash)
			return key.queryStablePerfectHash();
		else
			return key.queryUnstablePerfectHash();
	}
}
