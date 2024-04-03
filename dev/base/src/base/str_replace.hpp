/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <string>

namespace base {
	/**
	 * Replaces all occurrences of `from` with `to`
	 * Thanks to: https://stackoverflow.com/a/3418285.
	 * @param str source string
	 * @param from pattern to be erased
	 * @param to pattern to be put instead of `from`
	 */
	void strReplaceAll(std::string& str, const std::string& from, const std::string& to) {
		if (from.empty()) return;
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
		}
	}
}
