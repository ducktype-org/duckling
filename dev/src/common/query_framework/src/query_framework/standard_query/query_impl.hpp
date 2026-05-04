/**
 * @file query_impl.hpp
 * @brief Implementation of macros and templates used in the generation of query implementations.
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/misc/lazy_implies.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/internal/acd.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/cycle_handling/cycle_exception.hpp>
#include <query_framework/internal/query_errors.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/q_stats/q_stats.hpp>
#include <query_framework/query_errors.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>  // IWYU pragma: export
#include <query_framework/utils/query_hash.hpp>

#include <type_traits>  // IWYU pragma: export

namespace query::internal {

	/**
	 * @brief Internal function implementing the call to a query.
	 *
	 * @tparam QueryImplType Implementation Struct of a Query to call.
	 * @param key Query key
	 * @param from node id of caller
	 * @return QueryImplType::QResult
	 *
	 * @TODO: #1887 think about what should be here, and rename it to inner-layer.
	 */
	template<typename QueryImplType>
	auto standardQueryEntry(const typename QueryImplType::QKey& key) ->
		typename QueryImplType::QResult {
		using QueryIntType = QueryImplType::QueryType;

		CORE_DEV_LOG(Query, "[QUERY \"", QueryIntType::QUERY_DATA.name, "\"]: Enter.\n");

		const auto perfect_hash = perfectHashKey<QueryImplType::IS_HASH_STABLE>(key);

		[[maybe_unused]]
		std::conditional_t<USE_STATS, CallStatsObject, NoStats> stat_object{ QueryIntType::getID() };

		// @TODO: #2026 add note status static assertion if possible.

		CORE_ASSERT(
			QueryImplType::load(perfect_hash).empty(), "Cache should be empty in standardQueryEntry"
		);

		auto node_id = makeNodeID<QueryIntType>(key);
		auto context = ContextAccess::make(node_id);

		// @FUTURE: provide legit acd here
		ACD acd;

		// Before computing, try to reuse result from disk if available and safe to do so.
		// Conditions:
		//  - QueryImplType provides loadFromDisc(QKey) -> PResult
		//  - redGreenSweep(node_id) returns true (node and its deps are green in previous graph)
		if constexpr (QueryImplType::CAN_BE_LOADED_FROM_DISK) {
			if (ContextAccess::getState()->redGreenSweep(node_id) == QueryState::PrevColor::Green) {
				auto loaded = QueryImplType::loadFromDisc(key);

				if (loaded) {
					CORE_DEV_LOG(
						Query,
						"[QUERY \"",
						QueryIntType::QUERY_DATA.name,
						"\"]: Loading from disk.\n"
					);


					// Merge previous graph nodes into current graph
					// We merge only node_id and its dependencies
					ContextAccess::getState()->mergePreviousGraphIntoCurrentGraph(node_id);
					return QueryImplType::store(perfect_hash, loaded.value(), acd);
				}  // fall through to provide() if loading from disk failure

				CORE_DEV_LOG(
					Query,
					"[QUERY \"",
					QueryIntType::QUERY_DATA.name,
					"\"]: Query was marked green, but loading from disk failed.\n"
				);
			}
		}

		/*******************************************************************\
		| Now we enter a section, in which we actually compute the query    |
		| result by calling provide().                                      |
		\*******************************************************************/


		// @TODO: #2195 in the future we might want to guarantee that query operation are no-throw
		// apart from panics and similar stuff.
		// We for sure need more control of what happens if query operation throws.

		// PROLOG:
		// we put the node, it does not have any deps yet,
		// actual cycle checks are done in ctx.query
		// @TODO: #1887 might want to put it under one more layer of abstraction:
		ContextAccess::getState()->addGraphNode(node_id);
		ContextAccess::getState()->getActiveGraph()->putNode(node_id, &context);
		CORE_DEV_LOG(Query, "[QUERY \"", QueryIntType::QUERY_DATA.name, "\"]: Calculating.\n");

		// EPILOG
		// Use of defer here makes it also called when an exception is thrown.
		defer({
			// This happens after node is calculated, and we are all done
			CORE_DEV_LOG(Query, "[QUERY \"", QueryIntType::QUERY_DATA.name, "\"]: Done.\n");
			ContextAccess::getState()->getActiveGraph()->removeNode(node_id);
		});

		if constexpr (USE_STATS) stat_object.was_provide_call = true;

		try {
			if constexpr (QueryImplType::CAN_BE_LOADED_FROM_DISK && QueryImplType::USES_QRESULT) {
				auto provide_result = QueryImplType::provide(context, key);
				if (provide_result.hasFailed()) {
					// Here we need to delete artifact from disk
					QueryImplType::deleteFromDisc(key);
				}
				return QueryImplType::store(perfect_hash, provide_result, acd);
			} else {
				return QueryImplType::store(perfect_hash, QueryImplType::provide(context, key), acd);
			}
		} catch (const QueryCycleException& qce) {
			CORE_DEV_LOG(
				Query,
				"[QUERY \"",
				QueryIntType::QUERY_DATA.name,
				"\"]: Caught query cycle exception."
			);

			if constexpr (QueryImplType::USES_QRESULT and QueryImplType::ALLOW_CYCLES) {
				return QueryImplType::store(perfect_hash, query::Failed(), acd);

			} else {
				CORE_PANIC(
					"Query cycle detected in a query that does not use QResult or does not allow "
				    "cycles, and thus cannot "
					"recover from it.",
					"Query: ",
					QueryIntType::QUERY_DATA.name
				);
			}
		} catch (const QueryFailedException& qfe) {
			CORE_DEV_LOG(
				Query,
				"[QUERY \"",
				QueryIntType::QUERY_DATA.name,
				"\"]: Caught failed exception.\n",
				qfe.what()
			);
			// If provide throws we need to delete artifact from disk from prev compilation
			if constexpr (QueryImplType::CAN_BE_LOADED_FROM_DISK)
				QueryImplType::deleteFromDisc(key);

			if constexpr (QueryImplType::USES_QRESULT
			              && QueryImplType::CATCH_EXCEPTIONS_IF_USING_QRESULT) {
				return QueryImplType::store(perfect_hash, query::Failed(), acd);
			} else {
				CORE_PANIC(qfe.what());
			}
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
	struct QueryImplementation {
		/**
		 * Type of query interface struct (i.e. declaration struct).
		 */
		using QueryType = QueryType_tp;

		using QKey        = typename QueryType_tp::QKey;
		using QResult     = typename QueryType_tp::QResult;
		using QResWithACD = CacheEntry<QResult>;
		using PResult     = PResult_tp;
		using PResWithACD = CacheEntry<PResult>;
		using LoadResult  = base::Optional<QResWithACD>;

		// Some forwards used to simplify the code:
		constexpr static bool IS_HASH_STABLE = QueryType_tp::QUERY_DATA.usesStableHashing();
		constexpr static bool CAN_BE_LOADED_FROM_DISK
			= QueryType_tp::QUERY_DATA.tags.can_be_loaded_from_disk;
		constexpr static bool USES_QRESULT = QueryType_tp::QUERY_DATA.tags.uses_qresult;
		constexpr static bool CATCH_EXCEPTIONS_IF_USING_QRESULT
			= QueryType_tp::QUERY_DATA.tags.catch_exceptions_if_using_qresult;
		constexpr static bool ALLOW_CYCLES = QueryType_tp::QUERY_DATA.tags.allow_cycles;


		/**
		 * Type of the perfect key-hash values used in the query.
		 */
		using KHash = KHashSelector<QKey, IS_HASH_STABLE>;

		using Context = ::query::Context;

		/**
		 * Standard query function signatures:
		 *  static auto provide(Context& context, QKey key) -> PResult;
		 *  static auto load(KHash key_hash) -> LoadResult;
		 *  static auto store(KHash key_hash, PResult res, query::ACD acd) ->
		 * QResult;
		 */
	};

	/**
	 * Checks if query implementation provides loadFromDisc with correct signature.
	 * @note loadFromDisc should return Optional<PResult>. Empty optional is used to signal absence
	 * of on-disk artifact.
	 */
	template<typename Impl>
	concept HasLoadFromDiscWithSignature = requires(const typename Impl::QKey& key) {
		{ Impl::loadFromDisc(key) } -> std::same_as<base::Optional<typename Impl::PResult>>;
	};

	/**
	 * Checks if query implementation provides deleteFromDisc with correct signature.
	 * @note deleteFromDisc should return bool, if data was actually deleted it must handle the case
	 * where the is no data to delete.
	 */
	template<typename Impl>
	concept HasDeleteFromDiscWithSignature = requires(const typename Impl::QKey& q_key) {
		{ Impl::deleteFromDisc(q_key) } -> std::same_as<bool>;
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
#define INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(type)                                                                                    \
	auto type::QueryType::internal_query(const type::QKey& key) -> type::QResult {                                                         \
		return ::query::internal::standardQueryEntry<type>(key);                                                                           \
	}                                                                                                                                      \
	auto type::QueryType::internal_load(::query::QueryStableHash hash) -> type::QResult {                                                  \
		return type::load(type::KHash(hash)).value().data;                                                                                 \
	}                                                                                                                                      \
	auto type::QueryType::internal_erase(::query::QueryStableHash hash) -> bool {                                                          \
		return type::erase(type::KHash(hash));                                                                                             \
	}                                                                                                                                      \
	static_assert(                                                                                                                         \
		not std::is_reference_v<type::QResult>,                                                                                            \
		"Query result type should not be a reference (use CRef instead)"                                                                   \
	);                                                                                                                                     \
	static_assert(                                                                                                                       \
		std::is_copy_constructible_v<type::QResult>,                                                                                      \    \
		"Query result type should be copy constructible"                                                                                  \
	); \
	static_assert(                                                                                                                         \
		not std::is_reference_v<type::PResult>,                                                                                            \
		"Provider result type should not be a reference (use CRef instead)"                                                                \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		std::is_same_v<                                                                                                                    \
			std::invoke_result_t<decltype(type::store), type::KHash, type::PResult, ::query::ACD>,                                         \
			type::QResult>,                                                                                                                \
		"Bad store result."                                                                                                                \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		std::is_same_v<                                                                                                                    \
			std::invoke_result_t<decltype(type::provide), ::query::Context&, type::QKey>,                                                  \
			type::PResult>,                                                                                                                \
		"Bad provide result."                                                                                                              \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		not std::is_reference_v<type::QKey>,                                                                                               \
		"Query key type should not be a reference (use custom struct instead)"                                                             \
	);                                                                                                                                     \
                                                                                                                                           \
	static_assert(                                                                                                                         \
		std::is_same_v<std::invoke_result_t<decltype(type::load), type::KHash>, type::LoadResult>,                                         \
		"Bad load result."                                                                                                                 \
	);                                                                                                                                     \
	static_assert(type::QueryType::QUERY_DATA.verify().isOk(), "Query data is inconsistent.");                                             \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(type::QueryType::QUERY_DATA.usesUnstableHashing(), ::query::HasUnstablePerfectHash<type::QKey>),                      \
		"queryUnstablePerfectHash must be implemented and return u64 or Bit256"                                                            \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(type::QueryType::QUERY_DATA.usesStableHashing(), ::query::HasStablePerfectHash<type::QKey>),                          \
		"queryStablePerfectHash must be implemented and return QueryStableHash"                                                            \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(type::QueryType::QUERY_DATA.tags.can_be_loaded_from_disk, ::query::internal::HasLoadFromDiscWithSignature<type>),     \
		"loadFromDisk must be implemented for queries that are cached on disk"                                                             \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(type::QueryType::QUERY_DATA.tags.can_be_loaded_from_disk, ::query::internal::HasDeleteFromDiscWithSignature<type>),   \
		"deleteFromDisk must be implemented for queries that are cached on disk"                                                           \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(                                                                                                                      \
			type::QueryType::QUERY_DATA.tags.uses_qresult, ::query::IsQResult<type::PResult>::value                                        \
		),                                                                                                                                 \
		"PResult must be a QResult if uses_qresult is true"                                                                                \
	);                                                                                                                                     \
	static_assert(                                                                                                                         \
		LAZY_IMPLIES(                                                                                                                      \
			not type::QueryType::QUERY_DATA.tags.uses_qresult,                                                                             \
			not query::IsQResult<type::PResult>::value                                                                                     \
		),                                                                                                                                 \
		"PResult must not be a QResult if uses_qresult is false"                                                                           \
	);                                                                                                                                     \
	decltype(type::QueryType::id) type::QueryType::id                                                                                      \
		= ::query::internal::registerQuery(type::QueryType::QUERY_DATA);

/**
 * @brief Macro used to define boilerplate implementation elements of given Query. This is
 * something, that should be inserted right after query-implementation struct declaration.
 * @param type Name of the Query
 */
#define QUERY_IMPLEMENTATION_BOILERPLATE(query_type) \
	INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_##query_type)
