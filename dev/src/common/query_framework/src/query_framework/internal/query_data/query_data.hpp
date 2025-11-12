#pragma once

#include <string_view>

namespace query::internal {

	/**
	 * General type of the query.
	 */
	enum class QueryType {
		Normal,
		SideInput,
		Input,
		Dummy  //> Query with that type should never be called or implemented. This is used in
		       // incremental compilation when inserting dummy nodes to current graph from previous
		       // graph.
	};

	/**
	 * Struct holding universal meta data of each query type.
	 */
	struct QueryData final {
		QueryType        type;
		std::string_view name;

		constexpr QueryData(QueryType type, std::string_view name): type(type), name(name) {}

		constexpr QueryData(const QueryData&) = default;
	};
};
