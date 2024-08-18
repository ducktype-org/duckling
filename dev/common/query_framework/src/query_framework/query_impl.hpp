#pragma once

#include <utility>
#include <type_traits>
#include <base/optional.hpp>
#include <base/str_utils.hpp>
#include <diagnostic/logger.hpp>

#include "acd.hpp"
#include "query_int.hpp"
#include "dep_graph.hpp"
#include "query_id_provider.hpp"
#include "logs.hpp"
#include "node_making.hpp"

namespace query {

	namespace detail {

		/**
		 * @brief ContextType is type of a special object
		 * that query implementation use to perform three key operations:
		 * 	* call other query
		 *  * log
		 *  * report compiler error
		 *
		 * @FUTURE: there exist a concept of "custom context" types as
		 * a way to hack-in the query model. This however will most likely be
		 * discarded.
		 */
		struct ContextType {
		private:
			NodeID my_node;

			ContextType(NodeID my_node): my_node(my_node){};
			friend struct ContextMaker;

		public:
			// @TODO: add currently engaged query sanity check to context operations

			// @TODO: Make the context (and thus the logger) be propagated through query calls,
			// so that all queries run on the same file / in the same compilation thread / whatever
			// use a single, *non-static* logger object.
			static dia::Logger logger;

			ContextType(const ContextType&) = delete;
			ContextType(ContextType&&)      = delete;

			template<typename OthQuery>
			auto query(typename OthQuery::QKey key) -> decltype(auto) {
				NodeID dep_id = makeNodeID(OthQuery::id, key);
				dep_graph::addDependency(my_node, dep_id);

				return OthQuery::internal_query(key, my_node);
			}

			/**
			 * Log message to be shown to the user.
			 * @param message The dia::Message to be logged.
			 */
			void log(base::unique_ptr<dia::Message> message) { logger.log(std::move(message)); }
		};

		inline dia::Logger ContextType::logger{};

		/**
		 * @brief Internal helper struct used to create context
		 */
		struct ContextMaker {
			static auto make(NodeID my_node) { return ContextType(my_node); }
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
			log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Enter.\n"));

			if (auto v = QueryImplType::load(key)) {
				// @FUTURE: Add ACD check here...
				log(base::strConcat(
					"[QUERY \"", QueryImplType::QueryType::name, "\"]: Cached. Done.\n"
				));

				// @todo: This might bind & to a const&, via std::move "creating" &&.
				// It should works for all cases in our codebase,
				// but I'm not sure if it will work always and if it is
				// standardized behaviour.
				return std::move(v.value().data);
			} else {
				auto node_id = makeNodeID(QueryImplType::QueryType::id, key);
				auto context = ContextMaker::make(node_id);

				// @FUTURE: provide legit acd here
				ACD acd;

				// epilog:
				dep_graph::setEntry(node_id, from);
				log(base::strConcat(
					"[QUERY \"", QueryImplType::QueryType::name, "\"]: Calculating.\n"
				));

				// calculation:
				auto&& result
					= QueryImplType::store(key, QueryImplType::provide(context, key), acd);

				// prolog:
				dep_graph::setExit(node_id);
				log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Done.\n"));

				return result;
			}
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

		using Context = ::query::detail::ContextType;

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
	ImplementationOf_##query_type final: public query::QueryImplementation<query_type, PResult>

/**
 * @brief This is an internal query, and shouldn't be used directly. It used by
 * `QUERY_IMPLEMENTATION_BOILERPLATE` macro and creates necessary components for
 * macro-implementation structs.
 * @param type Name of a struct with query implementation
 * @param pretty_name Pretty name of the Query
 */
#define INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(type, pretty_name)                                \
	auto type::QueryType::internal_query(type::QueryType::QKey key, ::query::detail::NodeID from)   \
		-> type::QueryType::QResult {                                                               \
		return ::query::detail::standardQueryEntry<type>(std::move(key), from);                     \
	}                                                                                               \
	decltype(type::QueryType::id)   type::QueryType::id = ::query::detail::newQueryId(pretty_name); \
	decltype(type::QueryType::name) type::QueryType::name = pretty_name;

/**
 * @brief Macro used to define boilerplate implementation elements of given Query. This is
 * something, that should be inserted right after query-implementation struct declaration.
 * @param type Name of the Query
 */
#define QUERY_IMPLEMENTATION_BOILERPLATE(query_type) \
	INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_##query_type, #query_type)

/**
 * @brief Macro defining typical hash based cache for fast prototyping.
 * It caches PResults using base::HashMap in a way that references to them are unstable.
 * @future: change it to component, when proper query-component system will be introduced
 */
#define QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF                                                  \
	static inline base::                                                                       \
		HashMap<QKey, query::CacheEntry<PResult>, ::base::PerfectHashFunctor<QKey>>            \
				cache;                                                                         \
	static auto load(const QKey& key) -> LoadResult {                                          \
		if (auto&& copy = cache.atMaybe(key)) { return QResWithACD{ copy->data, copy->acd }; } \
		return {};                                                                             \
	}                                                                                          \
	static auto store(const QKey& key, PResult res, query::ACD acd) -> QResult {               \
		cache.put(key, { std::move(res), acd });                                               \
		return cache.at(key).data;                                                             \
	}

/**
 * @brief Macro defining typical hash based cache for fast prototyping.
 * It caches PResults using base::StableHashMap in a way that references to them are stable.
 * @future: change it to component, when proper query-component system will be introduced
 */
#define QUERY_AUTO_CACHE_PRESULT_STABLE_REF                                                    \
	static inline base::                                                                       \
		StableHashMap<QKey, query::CacheEntry<PResult>, ::base::PerfectHashFunctor<QKey>>      \
				cache;                                                                         \
	static auto load(const QKey& key) -> LoadResult {                                          \
		if (auto&& copy = cache.atMaybe(key)) { return QResWithACD{ copy->data, copy->acd }; } \
		return {};                                                                             \
	}                                                                                          \
	static auto store(const QKey& key, PResult res, query::ACD acd) -> QResult {               \
		cache.put(key, query::CacheEntry<PResult>{ std::move(res), acd });                     \
		return cache.at(key).data;                                                             \
	}


/**
 * @brief Macro defining empty storing and loading for when providing fresh result
 * is expected to be faster than trying to look it up in a cache.
 */
#define QUERY_AUTO_NO_CACHE                                                     \
	static auto store(const QKey&, PResult res, const query::ACD&) -> QResult { \
		return QResult{ std::move(res) };                                       \
	}                                                                           \
                                                                                \
	static auto load(const QKey&) -> LoadResult { return {}; }
