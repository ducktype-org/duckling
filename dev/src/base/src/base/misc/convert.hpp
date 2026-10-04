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
