#pragma once

#include "ints.hpp"
#include "string_id.hpp"
#include <charconv>

namespace base {
	/**
	 * @brief Converts a string to the value of the number it contains.
	 *
	 * Raises exception on error.
	 */
	i64 strIdToNum(base::StrId str) {
		auto view = str.strView();

		i64                    out{};
		std::from_chars_result res = std::from_chars(view.data(), view.data() + view.size(), out);
		if (res.ec == std::errc::invalid_argument)
			throw std::invalid_argument{ "invalid_argument" };
		else if (res.ec == std::errc::result_out_of_range)
			throw std::out_of_range{ "out_of_range" };

		return out;
	}
}
