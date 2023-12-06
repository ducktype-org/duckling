#pragma once

#include "ints.hpp"

#include <vector>
#include <concepts>

namespace base {
	/**
	 * @brief converts a number to a string of its hex notation
	 * 
	 * @param hex the number to convert
	 * @param length the exact number of hex digits in the output or 0 to get it from the length of the number
	 * @return std::string int the format `0xC0FFE`
	 */
	std::string toHexString(usize hex, usize length = 0) {
		if (length == 0) {
			usize cp = hex;
			while(cp) {
				cp /= 16;
				length++;
			}
			if (length == 0) {
				length = 1;
			}
		}
		std::string out(length + 2, '0');
		out[1] = 'x';
		usize i = 0, r;
		while (i < length && hex) {
			r = hex % 16;	
			out[length + 1 - i] = (r > 9) ? ('A' + r - 10) : ('0' + r);
			hex = hex / 16;
			i++;
		} 
		return out;
	}
}