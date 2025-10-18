#pragma once

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <type_traits>
#include <utility>

namespace query {
	using QueryStableHash = base::Bit256;

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

	template<typename KeyType>
	struct KHashSelector {
		using type
			= decltype(std::declval<std::remove_cvref_t<KeyType>>().queryUnstablePerfectHash());
	};

	template<>
	struct KHashSelector<u64> {
		using type = u64;
	};

	template<>
	struct KHashSelector<bool> {
		using type = u64;
	};

	template<typename KeyType>
	requires HasStablePerfectHash<std::remove_cvref_t<KeyType>> struct KHashSelector<KeyType> {
		using type = QueryStableHash;
	};

	template<typename KeyType>
	using KHash = typename KHashSelector<KeyType>::type;

	/**
	 * @brief Gets "perfect" hash from a key: prefer stable if queryStablePerfectHash exists,
	 * otherwise use queryUnstablePerfectHash. Keeps the u64/bool fast-paths.
	 */
	template<typename KeyType>
	KHash<KeyType> perfectHashKey(const KeyType& key) {
		if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, u64>)
			return key;
		else if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, bool>)
			return static_cast<u64>(key);
		else if constexpr (HasStablePerfectHash<std::remove_cvref_t<KeyType>>)
			return key.queryStablePerfectHash();
		else
			return key.queryUnstablePerfectHash();
	}
}
