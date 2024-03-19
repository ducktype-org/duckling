#pragma once

#include <base/optional.hpp>

#include "acd.hpp"

namespace query {

	struct ContextType {

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

	uint64_t nextQueryId();
}

// @TODO: proper context:
#define IMPLEMENT_QUERY_OF(type, pretty_name) \
	auto type::QueryType::query(type::QueryType::QKey key) -> type::QueryType::QResult { \
 		if (auto v = type::load(key)) {  \
			/*@TODO: Add ACD check here...*/ \
			return v.value().data;            \
		}                                \
		else { \
			type::Context context; \
			ACD acd; /*@TODO: provide acd here*/ \
			return type::store(key, type::provide(context, key), acd);  \
		} \
	} \
	decltype(Query1::id) Query1::id = ::query::nextQueryId();   \
	decltype(Query1::name) Query1::name = pretty_name;      \

	// auto type::QueryType::id = 0;
	// auto type::QueryType::name = "abc";


