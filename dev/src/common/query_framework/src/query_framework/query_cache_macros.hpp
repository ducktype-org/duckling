#pragma once

#include <concurrent/collections/hash_map.hpp>

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::HashMap and returns copies of results on cache hit.
 */
#define QUERY_AUTO_CACHE_COPY                                                   \
	static inline base::HashMap<KHash, query::CacheEntry<PResult>> cache;       \
	static auto load(KHash key_hash) -> LoadResult {                            \
		if (const auto& value = cache.atMaybe(key_hash)) {                      \
			return QResWithACD{ (*value)->data, (*value)->acd };                \
		}                                                                       \
		return {};                                                              \
	}                                                                           \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult { \
		cache.put(key_hash, { std::move(res), acd });                           \
		return cache.at(key_hash).data;                                         \
	}                                                                           \
	static_assert(                                                              \
		std::is_same_v<PResult, QResult>,                                       \
		"PResult and QResult should be equal for QUERY_AUTO_CACHE_COPY"         \
	);                                                                          \
	static_assert(                                                              \
		std::is_copy_constructible_v<PResult>,                                  \
		"PResult should be copy constructible for QUERY_AUTO_CACHE_COPY"        \
	);

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::HashMap and returns directly constructed QResults on cache hit.
 * @note Should not be used in place of QUERY_AUTO_CACHE_COPY for the sake of transparency.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT                                                      \
	static inline base::HashMap<KHash, query::CacheEntry<PResult>> cache;               \
	static auto load(KHash key_hash) -> LoadResult {                                    \
		if (const auto& value = cache.atMaybe(key_hash)) {                              \
			return QResWithACD{ QResult((*value)->data), (*value)->acd };               \
		}                                                                               \
		return {};                                                                      \
	}                                                                                   \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {         \
		cache.put(key_hash, { std::move(res), acd });                                   \
		return QResult(cache.at(key_hash).data);                                        \
	}                                                                                   \
	static_assert(                                                                      \
		std::is_constructible_v<QResult, PResult> && !std::is_same_v<QResult, PResult>, \
		"QResult should be constructible from (but not equal to) PResult for "          \
		"QUERY_AUTO_CACHE_CONSTRUCT"                                                    \
	);

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::HashMap and returns QResults constructed
 * from a CRef<PResult> on cache hit.
 * @note Should not be used in place of QUERY_AUTO_CACHE_CREF for the sake of transparency.
 * @note This macro acts similarly to QUERY_AUTO_CACHE_CREF, but additionally calls a constructor.
 *
 * @param bypass_constructible_check If set to true, bypasses the static_assert checks
 * ensuring that QResult is constructible from CRef<PResult>. Use with caution.
 * This is useful in scenarios when this macro works due to friendship, but is_constructible_v
 * fails, since it cannot see private constructors.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF(bypass_constructible_check)                   \
	static inline base::StableHashMap<KHash, query::CacheEntry<PResult>> cache;            \
	static auto load(KHash key_hash) -> LoadResult {                                       \
		if (auto value = cache.atMaybe(key_hash)) {                                        \
			return QResWithACD{ QResult(CRef<PResult>(&(*value)->data)), (*value)->acd };  \
		}                                                                                  \
		return {};                                                                         \
	}                                                                                      \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {            \
		auto ref = cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd }); \
		return QResult(CRef<PResult>(&ref->value.data));                                   \
	}                                                                                      \
	static_assert(                                                                         \
		!std::is_same_v<QResult, CRef<PResult>>,                                           \
		"QResult should not be equal to CRef<PResult> for "                                \
		"QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF"                                             \
	);                                                                                     \
	static_assert(                                                                         \
		bypass_constructible_check || std::is_constructible_v<QResult, CRef<PResult>>,     \
		"QResult should be constructible from (but not equal to) CRef<PResult> for "       \
		"QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF"                                             \
	);


/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::StableHashMap and returns stable references to results
 * on cache hit.
 */
#define QUERY_AUTO_CACHE_CREF                                                              \
	static inline concurrent::ConHashMap<KHash, query::CacheEntry<PResult>> cache;         \
	static auto load(KHash key_hash) -> LoadResult {                                       \
		if (auto value = cache.atMaybe(key_hash)) {                                        \
			return QResWithACD{ CRef<PResult>(&(*value)->data), (*value)->acd };           \
		}                                                                                  \
		return {};                                                                         \
	}                                                                                      \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {            \
		auto ref = cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd }); \
		return CRef<PResult>(&ref->value.data);                                            \
	}                                                                                      \
	static_assert(                                                                         \
		std::is_same_v<CRef<PResult>, QResult>,                                            \
		"QResult should be a CRef of PResult for QUERY_AUTO_CACHE_REF"                     \
	);


/**
 * @brief Macro defining empty storing and loading for when providing fresh result
 * is expected to be faster than trying to look it up in a cache.
 */
#define QUERY_AUTO_NO_CACHE                                                             \
	static auto store(KHash, PResult res, const query::ACD&) -> QResult {               \
		return QResult{ std::move(res) }; /* NOLINT(clang-diagnostic-redundant-move) */ \
	}                                                                                   \
                                                                                        \
	static auto load(KHash) -> LoadResult { return {}; }
