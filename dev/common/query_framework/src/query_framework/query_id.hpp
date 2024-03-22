#pragma once

#include <base/ints.hpp>

namespace query {
	struct QueryID {
	// private: @TODO
		u64 val;
		constexpr bool operator==(const QueryID&) const = default;
		constexpr u64 asInt() const { return val; }
	};
}

