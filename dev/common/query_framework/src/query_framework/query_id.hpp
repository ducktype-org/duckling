#pragma once

#include <base/maps.hpp>
#include <base/ints.hpp>

namespace query::detail {
	struct QueryID {
		using VAL_T = u64;
		VAL_T val;

		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}

		static void setName(const QueryID& query, std::string_view name);

		[[nodiscard]]
		const std::string& getName() const;
	};
}
