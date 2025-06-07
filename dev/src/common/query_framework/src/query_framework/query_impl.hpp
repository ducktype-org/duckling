/**
 * @file query_impl.hpp
 * @brief Implementation of macros and templates used in the generation of query implementations.
 */
#pragma once

#include "context.hpp"
#include "detail/acd.hpp"
#include "detail/context_access.hpp"
#include "detail/query_graph/node_making.hpp"
#include "detail/utils/logs.hpp"
#include "query_cache_macros.hpp"  // IWYU pragma: export
#include "query_hash.hpp"
#include "query_int.hpp"

#include <base/defer.hpp>
#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/stable_hashmap.hpp>
#include <base/str_utils.hpp>

#include <type_traits>  // IWYU pragma: export
#include <utility>

namespace query::detail {

	/**
	 * @brief Internal function implementing the call to a query.
	 *
	 * @tparam QueryImplType Implementation Struct of a Query to call.
	 * @param key Query key
	 * @param from node id of caller
	 * @return QueryImplType::QResult
	 */
	template<typename QueryImplType>
	auto standardQueryEntry(typename QueryImplType::QKey key, NodeID from) ->
		typename QueryImplType::QResult {
		QUERY_DEBUG_LOG("[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Enter.\n");

		const auto unstable_hash = unstableHashKey(key);

		if (auto v = QueryImplType::load(unstable_hash)) {
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

			// This is all at the end, with defer above,
			// to guarantee copy elision with "prvalue semantics".
			return QueryImplType::store(unstable_hash, QueryImplType::provide(context, key), acd);
		}
	}

	/**
	 * @brief Base class for Query implementation struct.
	 * The reason PResult is defined here is that in some cases
	 * it might allow to remove big dependencies from .hpp files.
	 *
	 * @tparam QueryType_tp Query to implement
	 * @tparam PResult_tp PResult of a query
	 */
	template<typename QueryType_tp, typename PResult_tp>
	struct QueryImplementation {
		using QueryType = QueryType_tp;

		using QKey        = typename QueryType_tp::QKey;
		using QResult     = typename QueryType_tp::QResult;
		using QResWithACD = CacheEntry<QResult>;
		using PResult     = PResult_tp;
		using PResWithACD = CacheEntry<PResult>;
		using LoadResult  = base::Optional<QResWithACD>;

		using Context = ::query::Context;

		/**
		 * Standard query function signatures:
		 *  static auto provide(Context& context, QKey key) -> PResult;
		 *  static auto load(query::QueryUnstableHash key_hash) -> LoadResult;
		 *  static auto store(query::QueryUnstableHash key_hash, PResult res, query::ACD acd) ->
		 * QResult;
		 */
		static constexpr bool CACHE_ON_DISK = false;
	};

}

/**
 * @brief Macro to be used as a struct signature when implementing a query.
 * @param query_type Name of the query
 * @param PResult Type returned by the Provide method
 */
#define IMPLEMENT_QUERY(query_type, PResult) \
	ImplementationOf_##query_type final:     \
		  public query::detail::QueryImplementation<query_type, PResult>

/**
 * @brief This is an internal query, and shouldn't be used directly. It used by
 * `QUERY_IMPLEMENTATION_BOILERPLATE` macro and creates necessary components for
 * macro-implementation structs.
 * @param type Name of a struct with query implementation
 * @param pretty_name Pretty name of the Query
 */
#define INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(type)                                           \
	auto type::QueryType::internal_query(type::QKey key, ::query::detail::NodeID from)            \
		-> type::QResult {                                                                        \
		return ::query::detail::standardQueryEntry<type>(std::move(key), from);                   \
	}                                                                                             \
	decltype(type::QueryType::id) type::QueryType::id                                             \
		= ::query::detail::registerQuery(type::QueryType::getData());                             \
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
				query::QueryUnstableHash,                                                         \
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
		"queryUnstablePerfectHash must be implemented and return u64 (query::QueryUnstableHash)." \
	);                                                                                            \
	static_assert(                                                                                \
		not type::CACHE_ON_DISK || ::query::HasStablePerfectHash<type::QKey>,                     \
		"If cache_on_disk is true, queryStablePerfectHash must be implemented and return Bit256 " \
		"(QueryStableHash)."                                                                      \
	);                                                                                            \
	static_assert(                                                                                \
		std::is_invocable_v<decltype(type::load), query::QueryUnstableHash>,                      \
		"Load function must be callable with query::QueryUnstableHash."                           \
	);

/**
 * @brief Macro used to define boilerplate implementation elements of given Query. This is
 * something, that should be inserted right after query-implementation struct declaration.
 * @param type Name of the Query
 */
#define QUERY_IMPLEMENTATION_BOILERPLATE(query_type) \
	INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_##query_type)
