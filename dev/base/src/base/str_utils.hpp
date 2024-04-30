/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#pragma once

#include "ints.hpp"
#include "string_id.hpp"

#include <charconv>
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
		split(const std::string& str, const std::string& delimeter = " ") {
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

	inline i64 strIdToNum(const base::StrId& str) {
		auto&& view = str.strView();

		i64 out;
		if (const auto [ptr, ec] = std::from_chars(view.data(), view.data() + view.size(), out);
		    ec == std::errc::invalid_argument)
			throw std::invalid_argument{ "invalid_argument" };
		else if (ec == std::errc::result_out_of_range)
			throw std::out_of_range{ "out_of_range" };

		return out;
	}
}
