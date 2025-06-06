#pragma once

/**
 * @brief Macro defining typical hash based cache for fast prototyping.
 * It caches PResults using base::HashMap and returns copies of results on cache hit.
 * @future: change it to component, when proper query-component system will be introduced
 */
#define QUERY_AUTO_CACHE_COPY                                                                      \
	static inline base::HashMap<query::UKHash<QueryType::QKey>, query::CacheEntry<PResult>> cache; \
	static auto load(query::UKHash<QueryType::QKey> key_hash) -> LoadResult {                      \
		if (const auto& value = cache.atMaybe(key_hash)) {                                         \
			return QResWithACD{ value->data, value->acd };                                         \
		}                                                                                          \
		return {};                                                                                 \
	}                                                                                              \
	static auto store(query::UKHash<QueryType::QKey> key_hash, PResult res, query::ACD acd)        \
		-> QResult {                                                                               \
		cache.put(key_hash, { std::move(res), acd });                                              \
		return cache.at(key_hash).data;                                                            \
	}                                                                                              \
	static_assert(                                                                                 \
		std::is_same_v<PResult, QResult>,                                                          \
		"PResult and QResult should be equal for QUERY_AUTO_CACHE_COPY"                            \
	);                                                                                             \
	static_assert(                                                                                 \
		std::is_copy_constructible_v<PResult>,                                                     \
		"PResult should be copy constructible for QUERY_AUTO_CACHE_COPY"                           \
	);


/**
 * @brief Macro defining typical hash based cache for fast prototyping.
 * It caches PResults using base::HashMap and returns directly constructed QResults on cache hit.
 * @note Cannot be used in place of QUERY_AUTO_CACHE_COPY for the sake of transparency.
 * @future: change it to component, when proper query-component system will be introduced
 */
#define QUERY_AUTO_CACHE_CONSTRUCT                                                                 \
	static inline base::HashMap<query::UKHash<QueryType::QKey>, query::CacheEntry<PResult>> cache; \
	static auto load(query::UKHash<QueryType::QKey> key_hash) -> LoadResult {                      \
		if (const auto& value = cache.atMaybe(key_hash)) {                                         \
			return QResWithACD{ value->data, value->acd };                                         \
		}                                                                                          \
		return {};                                                                                 \
	}                                                                                              \
	static auto store(query::UKHash<QueryType::QKey> key_hash, PResult res, query::ACD acd)        \
		-> QResult {                                                                               \
		cache.put(key_hash, { std::move(res), acd });                                              \
		return cache.at(key_hash).data;                                                            \
	}                                                                                              \
	static_assert(                                                                                 \
		std::is_constructible_v<QResult, PResult> && !std::is_same_v<QResult, PResult>,            \
		"QResult should be constructible from (but not equal to) PResult for "                     \
		"QUERY_AUTO_CACHE_CONSTRUCT"                                                               \
	);


/**
 * @brief Macro defining typical hash based cache for fast prototyping.
 * It caches PResults using base::StableHashMap and returns stable references to results
 * on cache hit.
 * @future: change it to component, when proper query-component system will be introduced
 */
#define QUERY_AUTO_CACHE_REF                                                                      \
	static inline base::StableHashMap<query::UKHash<QueryType::QKey>, query::CacheEntry<PResult>> \
				cache;                                                                            \
	static auto load(query::UKHash<QueryType::QKey> key_hash) -> LoadResult {                     \
		if (auto value = cache.atMaybe(key_hash)) {                                               \
			return QResWithACD{ CRef<PResult>(&value->data), value->acd };                        \
		}                                                                                         \
		return {};                                                                                \
	}                                                                                             \
	static auto store(query::UKHash<QueryType::QKey> key_hash, PResult res, query::ACD acd)       \
		-> QResult {                                                                              \
		cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd });                   \
		return CRef<PResult>(&cache[key_hash].data);                                              \
	}                                                                                             \
	static_assert(                                                                                \
		std::is_same_v<CRef<PResult>, QResult>,                                                   \
		"QResult should be a CRef of PResult for QUERY_AUTO_CACHE_REF"                            \
	);


/**
 * @brief Macro defining empty storing and loading for when providing fresh result
 * is expected to be faster than trying to look it up in a cache.
 */
#define QUERY_AUTO_NO_CACHE                                                                        \
	static auto store(query::UKHash<QueryType::QKey>, PResult res, const query::ACD&) -> QResult { \
		return QResult{ std::move(res) }; /* NOLINT(clang-diagnostic-redundant-move) */            \
	}                                                                                              \
                                                                                                   \
	static auto load(query::UKHash<QueryType::QKey>) -> LoadResult { return {}; }
