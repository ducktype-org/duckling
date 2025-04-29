#pragma once

#include <string_view>

namespace query::detail {

	enum class QueryType {
		Normal,
		SideInput,
		Input,
	};

	/**
	 * Struct holding some meta data of each query.
	 */
	struct QueryData {
		QueryType        type;
		std::string_view name;

		constexpr QueryData(QueryType type, std::string_view name): type(type), name(name) {}

		constexpr QueryData(const QueryData&) = default;
	};
};
