// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <concepts>
#include <utility>

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
