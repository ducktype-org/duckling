#pragma once

#include <string>
#include <vector>

namespace base {
	/**
	 * @brief Divides a list of arguments divided by commas into separate strings while ignoring any
	 * white spaces.
	 *
	 * @note This solution is somewhat over engineered.
	 */
	std::vector<std::string> vaArgSplit(std::string_view va_arg);
}
