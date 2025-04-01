/**
 * @file query_impl.hpp
 * @brief Implementation of macros and templates used in the generation of query implementations.
 */
#pragma once

#include <utility>
#include <type_traits>  // IWYU pragma: export
#include <base/optional.hpp>
#include <base/str_utils.hpp>
#include <base/defer.hpp>
#include <base/maps.hpp>
#include <base/stable_hashmap.hpp>
#include <base/ref.hpp>
#include <base/exceptions.hpp>

#include <base/defer.hpp>
#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/stable_hashmap.hpp>
#include <base/str_utils.hpp>

#include <type_traits>  // IWYU pragma: export
#include <utility>

#include "context.hpp"

namespace query::detail {
	/**
	 * @brief Internal helper struct used to create context
	 */
	struct ContextMaker final {
		static auto make(NodeID my_node) { return Context(my_node); }
	};

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

		if (auto v = QueryImplType::load(key)) {
			// @FUTURE: Add ACD check here...
			QUERY_DEBUG_LOG(
				"[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Cached. Done.\n"
			);

			// @todo: This might bind & to a const&, via std::move "creating" &&.
			// It should works for all cases in our codebase,
			// but I'm not sure if it will work always and if it is
			// standardized behaviour.
			return std::move(v.value().data);
		} else {
			auto node_id = makeNodeID(QueryImplType::QueryType::getID(), key);
			auto context = ContextMaker::make(node_id);

			// @FUTURE: provide legit acd here
			ACD acd;


			// Use of defer here makes it also called when an exception is thrown.
			// it is before setEntry, because setEntry can throw on cycle
			// @TODO: in the future we might want to guarantee that query operation are no-throw
			// apart from panics and similar stuff.
			// We for sure need more control of what happens if query operation throws.
			defer(dep_graph::setExit(node_id));

			// prolog:
			dep_graph::setEntry(node_id, from);

			QUERY_DEBUG_LOG(
				"[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Calculating.\n"
			);

			// epilog:
			defer(QUERY_DEBUG_LOG("[QUERY \"", QueryImplType::QueryType::getName(), "\"]: Done.\n")
			);

			// This is all at the end, with defer above,
			// to avoid false positive dangling reference warning.
			// We can't do it move-less without using temporary
			// lifetime extension, which causes the warning.
			return QueryImplType::store(key, QueryImplType::provide(context, key), acd);
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
		 *  static auto load(QKey key) -> LoadResult;
		 *  static auto store(QKey key, PResult res, query::ACD acd) -> QResult;
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
		  public query::detail::QueryImplementation<query_type, PResult>

/**
 * @brief This is an internal query, and shouldn't be used directly. It used by
 * `QUERY_IMPLEMENTATION_BOILERPLATE` macro and creates necessary components for
 * macro-implementation structs.
 * @param type Name of a struct with query implementation
 * @param pretty_name Pretty name of the Query
 */
#define INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(type, pretty_name)                                \
	auto type::QueryType::internal_query(type::QKey key, ::query::detail::NodeID from)              \
		-> type::QResult {                                                                          \
		return ::query::detail::standardQueryEntry<type>(std::move(key), from);                     \
	}                                                                                               \
	decltype(type::QueryType::id)   type::QueryType::id = ::query::detail::newQueryID(pretty_name); \
	decltype(type::QueryType::name) type::QueryType::name = pretty_name;                            \
	static_assert(                                                                                  \
		not std::is_reference_v<type::QResult>,                                                     \
		"Query result type should not be a reference (use CRef instead)"                            \
	);                                                                                              \
	static_assert(                                                                                  \
		not std::is_reference_v<type::PResult>,                                                     \
		"Provider result type should not be a reference (use CRef instead)"                         \
	);                                                                                              \
	static_assert(                                                                                  \
		std::is_same_v<                                                                             \
			std::invoke_result_t<decltype(type::store), type::QKey, type::PResult, ::query::ACD>,   \
			type::QResult>,                                                                         \
		"Bad store result."                                                                         \
	);                                                                                              \
	static_assert(                                                                                  \
		std::is_same_v<                                                                             \
			std::invoke_result_t<decltype(type::provide), ::query::Context&, type::QKey>,           \
			type::PResult>,                                                                         \
		"Bad provide result."                                                                       \
	);


/**
 * @brief Macro used to define boilerplate implementation elements of given Query. This is
 * something, that should be inserted right after query-implementation struct declaration.
 * @param type Name of the Query
 */
#define QUERY_IMPLEMENTATION_BOILERPLATE(query_type) \
	INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_##query_type, #query_type)
