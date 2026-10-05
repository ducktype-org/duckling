// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

#include <string>

namespace base {
	/**
	 * @brief converts a number to a string of its hex notation
	 *
	 * @param hex the number to convert
	 * @param length the exact number of hex digits in the output or 0 to get it from the length of
	 * the number
	 * @return std::string int the format `0xC0FFE`
	 */
	std::string toHexString(usize hex, usize length = 0);
}
