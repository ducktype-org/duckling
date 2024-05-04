/**
 * @file str_replace.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#pragma once

#include <string>
#include <vector>

namespace base {
	/**
	 * Replaces all occurrences of `from` with `to`
	 * @param str source string
	 * @param from pattern to be erased
	 * @param to pattern to be put instead of `from`
	 */
	void strReplaceAll(std::string& str, const std::string& from, const std::string& to);

	/**
	 * Splits a string by a delimeter.
	 * @param str The string to split
	 * @param delimeter A string, that is used to separate the substrings.
	 * @return A vector of separated strings.
	 */
	std::vector<std::string> strSplit(const std::string& str, const std::string& delimeter = " ");
}
