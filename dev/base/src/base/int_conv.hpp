#pragma once

#include <concepts>
#include <utility>

#include "exceptions.hpp"

namespace base {
	/**
	 * Convert given integer value to
	 * different integral type.
	 * Panics if conversion would change the value.
	 */
	template<std::integral T, std::integral U>
	T safeIntConv(U u) {
		CORE_ASSERT(std::in_range<T>(u), "Bad integer conversion");
		return static_cast<T>(u);
	}
}
