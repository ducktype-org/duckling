#pragma once

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/collections/absl_hash_map.hpp>

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::HashMap and returns copies of results on cache hit.
 */
#define QUERY_AUTO_CACHE_COPY                                                      \
	static inline concurrent::AbslConHashMap<KHash, query::CacheEntry<PResult>> cache; \
	static auto load(KHash key_hash) -> LoadResult {                               \
		if (const auto& value = cache.atMaybeCopy(key_hash)) {                     \
			return QResWithACD{ (*value).data, (*value).acd };                     \
		}                                                                          \
		return {};                                                                 \
	}                                                                              \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {    \
		cache.put(key_hash, { std::move(res), acd });                              \
		return cache.at(key_hash)->data;                                           \
	}                                                                              \
	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash); }    \
	static_assert(                                                                 \
		std::is_same_v<PResult, QResult>,                                          \
		"PResult and QResult should be equal for QUERY_AUTO_CACHE_COPY"            \
	);                                                                             \
	static_assert(                                                                 \
		std::is_copy_constructible_v<PResult>,                                     \
		"PResult should be copy constructible for QUERY_AUTO_CACHE_COPY"           \
	);

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::HashMap and returns directly constructed QResults on cache hit.
 * @note Should not be used in place of QUERY_AUTO_CACHE_COPY for the sake of transparency.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT                                                      \
	static inline concurrent::AbslConHashMap<KHash, query::CacheEntry<PResult>> cache;      \
	static auto load(KHash key_hash) -> LoadResult {                                    \
		if (const auto& value = cache.atMaybe(key_hash)) {                              \
			return QResWithACD{ QResult((*value)->data), (*value)->acd };               \
		}                                                                               \
		return {};                                                                      \
	}                                                                                   \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {         \
		cache.put(key_hash, { std::move(res), acd });                                   \
		return QResult(cache.at(key_hash)->data);                                       \
	}                                                                                   \
	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash); }         \
	static_assert(                                                                      \
		std::is_constructible_v<QResult, PResult> && !std::is_same_v<QResult, PResult>, \
		"QResult should be constructible from (but not equal to) PResult for "          \
		"QUERY_AUTO_CACHE_CONSTRUCT"                                                    \
	);

/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::StableHashMap and returns QResults constructed
 * from a CRef<PResult> on cache hit.
 * @note Should not be used in place of QUERY_AUTO_CACHE_CREF for the sake of transparency.
 * @note This macro acts similarly to QUERY_AUTO_CACHE_CREF, but additionally calls a constructor.
 *
 * @note Implementation uses QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK
 * defined below, which is all backwards, but it avoids code duplication.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF                                         \
	QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK               \
	static_assert(                                                                   \
		std::is_constructible_v<QResult, CRef<PResult>>,                             \
		"QResult should be constructible from (but not equal to) CRef<PResult> for " \
		"QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF"                                       \
	);


/**
 * @brief Same as QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF, but it doesn't include the static_assert
 * checks ensuring that QResult is constructible from CRef<PResult>. Use with caution. This is
 * useful in scenarios when this macro works due to friendship, but is_constructible_v fails, since
 * it cannot see private constructors.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK                 \
	static inline concurrent::AbslConHashMap<KHash, query::CacheEntry<PResult>> cache;         \
	static auto load(KHash key_hash) -> LoadResult {                                       \
		if (auto value = cache.atMaybe(key_hash)) {                                        \
			return QResWithACD{ QResult(CRef<PResult>(&(*value)->data)), (*value)->acd };  \
		}                                                                                  \
		return {};                                                                         \
	}                                                                                      \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {            \
		auto ref = cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd }); \
		return QResult(CRef<PResult>(&ref->second.data));                                   \
	}                                                                                      \
	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash); }            \
	static_assert(                                                                         \
		!std::is_same_v<QResult, CRef<PResult>>,                                           \
		"QResult should not be equal to CRef<PResult> for "                                \
		"QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK"               \
	);


/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::StableHashMap and returns stable references to results
 * on cache hit.
 */
#define QUERY_AUTO_CACHE_CREF                                                              \
	static inline concurrent::AbslConHashMap<KHash, query::CacheEntry<PResult>> cache;         \
	static auto load(KHash key_hash) -> LoadResult {                                       \
		if (auto value = cache.atMaybe(key_hash)) {                                        \
			return QResWithACD{ CRef<PResult>(&(*value)->data), (*value)->acd };           \
		}                                                                                  \
		return {};                                                                         \
	}                                                                                      \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {            \
		auto ref = cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd }); \
		return CRef<PResult>(&ref->second.data);                                            \
	}                                                                                      \
	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash); }            \
	static_assert(                                                                         \
		std::is_same_v<CRef<PResult>, QResult>,                                            \
		"QResult should be a CRef of PResult for QUERY_AUTO_CACHE_REF"                     \
	);


/**
 * @brief Macro defining typical hash based cache.
 * It caches PResults using base::StableHashMap and returns QResults constructed
 * from a CRef<PResult> by the provided lambda function.
 *
 * @important lambda is called on both cache hit (load) and cache miss (store).
 *
 * @note This macro acts similarly to QUERY_AUTO_CACHE_CREF, but additionally calls provided lambda.
 */
#define QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(lambda)                                               \
	static inline concurrent::AbslConHashMap<KHash, query::CacheEntry<PResult>> cache;                 \
	static auto load(KHash key_hash) -> LoadResult {                                               \
		static constexpr auto construct_lambda = lambda;                                           \
                                                                                                   \
		if (auto value = cache.atMaybe(key_hash)) {                                                \
			return QResWithACD{ construct_lambda(CRef<PResult>(&(*value)->data)), (*value)->acd }; \
		}                                                                                          \
		return {};                                                                                 \
	}                                                                                              \
	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {                    \
		static constexpr auto construct_lambda = lambda;                                           \
                                                                                                   \
		auto ref = cache.put(key_hash, query::CacheEntry<PResult>{ std::move(res), acd });         \
		return construct_lambda(&ref->second.data);                                                 \
	}                                                                                              \
	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash); }                    \
	static_assert(                                                                                 \
		not std::is_same_v<QResult, CRef<PResult>>,                                                \
		"QResult should not be equal to CRef<PResult> for "                                        \
		"QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA"                                                     \
	);
