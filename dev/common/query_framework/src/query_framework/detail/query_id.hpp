/**
 * @file query_id.hpp
 * @brief Definition of query id type.
 */
#pragma once

#include <base/ints.hpp>

#include <string_view>

namespace query::detail {
	struct QueryID final {
		using VAL_T = u64;
		VAL_T val;

		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}

		static void setName(QueryID query, std::string_view name);

		[[nodiscard]]
		std::string_view getName() const;

		[[nodiscard]]
		constexpr bool operator==(const QueryID& other) const {
			return val == other.val;
		}
	};
}
