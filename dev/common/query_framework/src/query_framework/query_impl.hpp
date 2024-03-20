#pragma once

#include <base/optional.hpp>

#include "acd.hpp"
#include "query_int.hpp"

namespace query {

	struct ContextType {

		template<typename OthQuery>
		auto query(typename OthQuery::QKey key) -> auto {
			// ....
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
		if (auto v = QueryImplType::load(key)) {
			/*@TODO: Add ACD check here...*/
			return v.value().data;
		}
		else {
			typename QueryImplType::Context context;
			ACD acd; /*@TODO: provide acd here*/
			return QueryImplType::store(key, QueryImplType::provide(context, key), acd);
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


