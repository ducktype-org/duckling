#pragma once

#include <base/optional.hpp>
#include <base/str_concat.hpp>

#include "acd.hpp"
#include "query_int.hpp"
#include "dep_graph.hpp"
#include "logs.hpp"

namespace query {

	struct ContextType {

		NodeID my_node;

		template<typename OthQuery>
		auto query(typename OthQuery::QKey key) -> auto {
			
			NodeID dep_id = makeNodeID(OthQuery::id, key);
			dep_graph::addDependency(my_node, dep_id);

			return OthQuery::query(key);
		}

		// @TODO: log

		// @TODO: error
	};

	template<
		typename QueryType_tp,
		typename PResult_tp
	>
	struct QueryImplementation {
		using QueryType = QueryType_tp;
		using QKey = typename QueryType_tp::QKey;
		
		using QResult = typename QueryType_tp::QResult;
		using PResult = PResult_tp;
		
		using Context = ::query::ContextType;

		using QResWithACD = AddACD<QResult>;

		using LoadRes = base::Optional<QResWithACD >;

		// static auto provide(QKey, Context&) -> PResult;
		// static auto load(QKey key) -> base::Optional<QResult>;
		// static auto store(QKey key, PResult) -> QResult;
	};

	/**
	 * @brief A simple counter for providing unique query id-s.
	 */
	QueryID nextQueryId();


	template<typename QueryImplType>
	auto standardQueryEntry(typename QueryImplType::QKey key) -> QueryImplType::QResult {
		log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Enter.\n"));
		
		if (auto v = QueryImplType::load(key)) {
			/*@TODO: Add ACD check here...*/
			log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Cached. Done.\n"));
			return v.value().data;
		}
		else {
			auto node_id = makeNodeID(QueryImplType::QueryType::id, key);

			typename QueryImplType::Context context{node_id};
			ACD acd; /*@TODO: provide acd here*/

			// epilog:
			dep_graph::setEntry(node_id);
			log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Calculating.\n"));
			
			// calculation:
			auto result = QueryImplType::store(key, QueryImplType::provide(context, key), acd);
			
			// prolog:
			dep_graph::setExit(node_id);
			log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Done.\n"));
			
			
			return result;
		}
	}
}

// @TODO: proper context:
#define IMPLEMENT_QUERY_OF(type, pretty_name) \
	auto type::QueryType::query(type::QueryType::QKey key) -> type::QueryType::QResult { \
 		return ::query::standardQueryEntry<type>(key);                                   \
	}                                                                                    \
	decltype(type::QueryType::id) type::QueryType::id = ::query::nextQueryId();          \
	decltype(type::QueryType::name) type::QueryType::name = pretty_name;


