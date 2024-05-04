/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#pragma once

#include <string>
#include <vector>

namespace base {
	/**
	 * Replaces all occurrences of `from` with `to`
	 * Thanks to: https://stackoverflow.com/a/3418285.
	 * @param str source string
	 * @param from pattern to be erased
	 * @param to pattern to be put instead of `from`
	 */
	inline void strReplaceAll(std::string& str, const std::string& from, const std::string& to) {
		if (from.empty()) return;
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
		}
	}

	/**
	 * Splits a string by a delimeter.
	 * @param str The string to split
	 * @param delimeter A string, that is used to separate the substrings.
	 * @return A vector of separated strings.
	 */
	inline std::vector<std::string>
		strSplit(const std::string& str, const std::string& delimeter = " ") {
		std::vector<std::string> result;
		size_t                   end_pos   = 0;
		size_t                   start_pos = 0;
		while ((end_pos = str.find(delimeter, start_pos)) != std::string::npos) {
			result.push_back(str.substr(start_pos, end_pos - start_pos));
			start_pos = end_pos + delimeter.length();
		}
		result.push_back(str.substr(start_pos));
		return result;
	}
}
