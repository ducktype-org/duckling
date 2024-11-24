/**
 * @file query_id.hpp
 * @brief Definition of query id type.
 */
#pragma once

#include <base/ints.hpp>

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
	};

	/**
	 * @brief A simple counter for providing unique query id-s.
	 * This function should never be used outside the framework.
	 * @param pretty_name A name for the new query.
	 */
	QueryID newQueryID(std::string_view pretty_name);

	/**
	 * @brief Provides query id of "outside world" query.
	 * This function should never be used outside the framework.
	 */
	QueryID outsideWorldQueryID();
}
