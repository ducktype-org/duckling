#pragma once

#include <string_view>

namespace query {
	
	template<
		typename QueryType_tp,
		typename QKey_tp,
		typename QResult_tp
		// context?
		// cache?
	>
	struct QueryInterface {
		using QueryType = QueryType_tp;
		using QResult = QResult_tp;
		using QKey = QKey_tp;
	};
}

#define QUERY_INTERFACE_BOILERPLATE \
	static auto query(QKey) -> QResult; \
	static std::string_view name;  \
	static uint64_t id;

