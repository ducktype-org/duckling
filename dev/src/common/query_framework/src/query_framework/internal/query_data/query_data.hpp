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
	};

	/**
	 * Struct holding universal meta data of each query type.
	 */
	struct QueryData {
		QueryType        type;
		std::string_view name;

		constexpr QueryData(QueryType type, std::string_view name): type(type), name(name) {}

		constexpr QueryData(const QueryData&) = default;
	};
};
