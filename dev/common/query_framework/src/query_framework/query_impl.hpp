#pragma once

#include <base/optional.hpp>

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

		// static auto provider(QKey, Context&) -> PResult;
		// static auto load(QKey key) -> base::Optional<QResult>;
		// static auto store(QKey key, PResult) -> QResult;
	};

}

#define IMPLEMENT_QUERY_OF(type) \
	auto type::QueryType::query(type::QueryType::QKey key) -> type::QueryType::QResult { \
 \
	}


