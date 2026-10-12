// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

	template<typename KeyType, bool use_stable_hash>
	struct KHashSelectorHelper final {
		using type = QueryStableHash;
	};

	template<typename KeyType>
	struct KHashSelectorHelper<KeyType, false> final {
		using type
			= decltype(std::declval<std::remove_cvref_t<KeyType>>().queryUnstablePerfectHash());
	};

	template<typename KeyType, bool USE_STABLE_HASH>
	using KHashSelector = typename KHashSelectorHelper<KeyType, USE_STABLE_HASH>::type;

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
