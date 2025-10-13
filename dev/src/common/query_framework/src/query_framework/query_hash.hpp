#pragma once

#include <base/bit256.hpp>
#include <base/ints.hpp>

#include <type_traits>
#include <utility>

namespace query {
	using QueryStableHash = base::Bit256;

	// Trait: whether a type has a static constexpr CACHE_ON_DISK and whether it is true
	template<typename T, typename = void>
	struct HasCacheOnDisk: std::false_type {};

	template<typename T>
	struct HasCacheOnDisk<T, std::void_t<decltype(T::CACHE_ON_DISK)>>:
		  std::bool_constant<static_cast<bool>(T::CACHE_ON_DISK)> {};

	/**
	 * @brief Helper struct to determine the hash type for a given KeyType.
	 * Specializations handle cases where KeyType is u64, bool, or has perfect-hash methods.
	 */
	template<typename KeyType, bool IsU64OrBool>
	struct KHashHelper;

	// For u64/bool we keep u64
	template<typename KeyType>
	struct KHashHelper<KeyType, true> {
		using type = u64;
	};

	// For other keys: if KeyType::CACHE_ON_DISK == true -> stable (Bit256),
	// otherwise use the return type of queryUnstablePerfectHash().
	template<typename KeyType>
	struct KHashHelper<KeyType, false> {
		using Clean = std::remove_cvref_t<KeyType>;
		using type  = std::conditional_t<
			 HasCacheOnDisk<Clean>::value,
			 QueryStableHash,
			 decltype(std::declval<Clean>().queryUnstablePerfectHash())>;
	};

	/**
	 * @brief Type alias to determine the hash type for a given KeyType.
	 */
	template<typename KeyType>
	using KHash = typename KHashHelper<
		KeyType,
		std::is_same_v<std::remove_cvref_t<KeyType>, u64>
			|| std::is_same_v<std::remove_cvref_t<KeyType>, bool>>::type;

	/**
	 * @brief Gets "perfect" hash from a key: stable if CACHE_ON_DISK==true,
	 * otherwise unstable. Keeps the u64/bool fast-paths.
	 */
	template<typename KeyType>
	KHash<KeyType> perfectHashKey(const KeyType& key) {
		if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, u64>)
			return key;
		else if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, bool>)
			return static_cast<u64>(key);
		else if constexpr (HasCacheOnDisk<std::remove_cvref_t<KeyType>>::value)
			return key.queryStablePerfectHash();
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
