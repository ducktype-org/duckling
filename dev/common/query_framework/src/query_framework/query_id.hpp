#pragma once

#include <base/ints.hpp>

namespace query::detail {
	struct QueryID {
		// private: @TODO
		u64 val;

		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}
	};
}
