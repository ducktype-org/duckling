#pragma once

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <type_traits>
#include <utility>

namespace query {
	using QueryStableHash = base::Bit256;

	/**
	 *  Concept to validate queryUnstablePerfectHash signature
	 */
	template<typename KeyType>
	concept HasUnstablePerfectHash
		= requires(KeyType t) {
			  { t.queryUnstablePerfectHash() } -> std::same_as<u64>;
		  } || requires(KeyType t) {
			  { t.queryUnstablePerfectHash() } -> std::same_as<base::Bit256>;
		  };

	/** Concept to validate queryStablePerfectHash signature */
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
	using KHashUnstable = decltype(std::declval<std::remove_cvref_t<KeyType>>().queryStablePerfectHash());

	/**
	 * @brief Gets "perfect" hash from a key: prefer stable if queryStablePerfectHash exists,
	 * otherwise use queryUnstablePerfectHash. Keeps the u64/bool fast-paths.
	 */
	template<typename KeyType, bool use_stable_hash>
	auto perfectHashKey(const KeyType& key) {
		if constexpr (use_stable_hash)
			return key.queryStablePerfectHash();
		else
			return key.queryUnstablePerfectHash();
	}
}
