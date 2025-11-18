/**
 * @file query_impl.hpp
 * @brief Implementation of macros and templates used in the generation of query implementations.
 */
#pragma once

#include "context.hpp"
#include "internal/acd.hpp"
#include "internal/context_access.hpp"
#include "internal/query_graph/node_making.hpp"
#include "internal/utils/logs.hpp"
#include "q_stats/q_stats.hpp"
#include "query_cache_macros.hpp"  // IWYU pragma: export
#include "query_hash.hpp"
#include "query_int.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <type_traits>  // IWYU pragma: export
#include <utility>

namespace query::internal {

	// Concept: implementation provides loadFromDisc with either exact PResult signature
	// or Optional<PResult> signature (used to signal absence of on-disk artifact).
	// @TODO: #1433 replace this with proper query tag
	template<typename Impl>
	concept HasLoadFromDiscWithSignature = requires(const typename Impl::QKey& key) {
		{ Impl::loadFromDisc(key) };
	} && (std::same_as<decltype(Impl::loadFromDisc(std::declval<const typename Impl::QKey&>())), base::Optional<typename Impl::PResult>>);

	/**
	 * @brief Internal function implementing the call to a query.
	 *
	 * @tparam QueryImplType Implementation Struct of a Query to call.
	 * @param key Query key
	 * @param from node id of caller
	 * @return QueryImplType::QResult
	 */
	template<typename QueryImplType>
	auto standardQueryEntry(const typename QueryImplType::QKey& key, NodeID from) ->
		typename QueryImplType::QResult {
		QUERY_DEBUG_LOG("[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Enter.\n");

		const auto perfect_hash = perfectHashKey(key);

		[[maybe_unused]]
		std::conditional_t<USE_STATS, CallStatsObject, NoStats> stat_object{
			QueryImplType::QueryType::getID()
		};


		if (auto v = QueryImplType::load(perfect_hash)) {
			// @FUTURE: Add ACD check here...
			QUERY_DEBUG_LOG(
				"[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Cached. Done.\n"
			);

			return std::move(v.value().data);
		} else {
			auto node_id = makeNodeID(QueryImplType::QueryType::getID(), key);
			auto context = ContextAccess::make(node_id);

			// @FUTURE: provide legit acd here
			ACD acd;

			// Before computing, try to reuse result from disk if available and safe to do so.
			// Conditions:
			//  - QueryImplType provides loadFromDisc(QKey) -> PResult
			//  - redGreenSweep(node_id) returns true (node and its deps are green in previous graph)
			if constexpr (HasLoadFromDiscWithSignature<QueryImplType>) {
				if (ContextAccess::getState()->redGreenSweep(node_id)
				    == QueryState::PrevColor::Green) {
					QUERY_DEBUG_LOG(
						"[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Loading from disk.\n"
					);

					auto loaded = QueryImplType::loadFromDisc(key);

					if (loaded) {
						// Merge previous graph nodes into current graph
						// We merge only node_id and its dependencies
						ContextAccess::getState()->mergePreviousGraphIntoCurrentGraph(node_id);
						return QueryImplType::store(perfect_hash, loaded.value(), acd);
					}  // fall through to provide() if loading from disk failure
				}
			}


			// Use of defer here makes it also called when an exception is thrown.
			// it is before setEntry, because setEntry can throw on cycle
			// @TODO: in the future we might want to guarantee that query operation are no-throw
			// apart from panics and similar stuff.
			// We for sure need more control of what happens if query operation throws.
			defer(ContextAccess::getState()->setExit(node_id));

			// prolog:
			ContextAccess::getState()->setEntry(node_id, from);

			QUERY_DEBUG_LOG("[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Calculating.\n");

			// epilog:
			defer(QUERY_DEBUG_LOG("[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Done.\n"));

			if constexpr (USE_STATS) stat_object.was_provide_call = true;

			// This is all at the end, with defer above,
			// to guarantee copy elision with "prvalue semantics".
			return QueryImplType::store(perfect_hash, QueryImplType::provide(context, key), acd);
		}
	}

	/**
	 * @brief Base class for Query implementation struct.
	 * The reason PResult is defined here is that in some cases
	 * it might allow to remove big dependencies from .hpp files.
	 *
	 * @tparam QueryType_tp Interface struct of Query to implement
	 * @tparam PResult_tp PResult of a query
	 */
	template<typename QueryType_tp, typename PResult_tp>
	struct QueryImplementation final {
		using QueryType = QueryType_tp;

		using QKey        = typename QueryType_tp::QKey;
		using QResult     = typename QueryType_tp::QResult;
		using QResWithACD = CacheEntry<QResult>;
		using PResult     = PResult_tp;
		using PResWithACD = CacheEntry<PResult>;
		using LoadResult  = base::Optional<QResWithACD>;
		
		/**
		 * Type of the perfect key-hash values used in the query.
		 */
		using KHash = std::conditional_t<
			QueryType_tp::QUERY_DATA.tags.isHashStable(),
			query::KHashStable<QKey>,
			query::KHashUnstable<QKey>
		>;

		using Context = ::query::Context;

		/**
		 * Standard query function signatures:
		 *  static auto provide(Context& context, QKey key) -> PResult;
		 *  static auto load(KHash key_hash) -> LoadResult;
		 *  static auto store(KHash key_hash, PResult res, query::ACD acd) ->
		 * QResult;
		 */
	};
}

/**
 * @brief Macro to be used as a struct signature when implementing a query.
 * @param query_type Name of the query
 * @param PResult Type returned by the Provide method
 */
#define IMPLEMENT_QUERY(query_type, PResult) \
	ImplementationOf_##query_type final:     \
		  public query::internal::QueryImplementation<query_type, PResult>

/**
 * @brief This is an internal query, and shouldn't be used directly. It used by
 * `QUERY_IMPLEMENTATION_BOILERPLATE` macro and creates necessary components for
 * macro-implementation structs.
 * @param type Name of a struct with query implementation
 * @param pretty_name Pretty name of the Query
 */
#define INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(type)                                           \
	auto type::QueryType::internal_query(const type::QKey& key, ::query::internal::NodeID from)   \
		-> type::QResult {                                                                        \
		return ::query::internal::standardQueryEntry<type>(key, from);                            \
	}                                                                                             \
	static_assert(                                                                                \
		not std::is_reference_v<type::QResult>,                                                   \
		"Query result type should not be a reference (use CRef instead)"                          \
	);                                                                                            \
	static_assert(                                                                                \
		not std::is_reference_v<type::PResult>,                                                   \
		"Provider result type should not be a reference (use CRef instead)"                       \
	);                                                                                            \
	static_assert(                                                                                \
		std::is_same_v<                                                                           \
			std::invoke_result_t<                                                                 \
				decltype(type::store),                                                            \
				query::KHash<type::QKey>,                                                         \
				type::PResult,                                                                    \
				::query::ACD>,                                                                    \
			type::QResult>,                                                                       \
		"Bad store result."                                                                       \
	);                                                                                            \
	static_assert(                                                                                \
		std::is_same_v<                                                                           \
			std::invoke_result_t<decltype(type::provide), ::query::Context&, type::QKey>,         \
			type::PResult>,                                                                       \
		"Bad provide result."                                                                     \
	);                                                                                            \
	static_assert(                                                                                \
		not std::is_reference_v<type::QKey>,                                                      \
		"Query key type should not be a reference (use custom struct instead)"                    \
	);                                                                                            \
	static_assert(                                                                                \
		::query::HasUnstablePerfectHash<type::QKey>,                                              \
		"queryUnstablePerfectHash must be implemented and return u64 or Bit256"                   \
	);                                                                                            \
	static_assert(                                                                                \
		not type::CACHE_ON_DISK || ::query::HasStablePerfectHash<type::QKey>,                     \
		"If cache_on_disk is true, queryStablePerfectHash must be implemented and return Bit256 " \
		"(QueryStableHash)."                                                                      \
	);                                                                                            \
	static_assert(                                                                                \
		std::is_invocable_v<decltype(type::load), query::KHash<type::QKey>>,                      \
		"Load function must be callable with hash of QKey"                                        \
	);                                                                                            \
	decltype(type::QueryType::id) type::QueryType::id = ::query::internal::                       \
		registerQuery(type::QueryType::QUERY_DATA, ::query::HasStablePerfectHash<type::QKey>);

/**
 * @brief Macro used to define boilerplate implementation elements of given Query. This is
 * something, that should be inserted right after query-implementation struct declaration.
 * @param type Name of the Query
 */
#define QUERY_IMPLEMENTATION_BOILERPLATE(query_type) \
	INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_##query_type)
