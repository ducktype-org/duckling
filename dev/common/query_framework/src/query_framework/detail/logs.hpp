/**
 * @file logs.hpp
 * @brief Implementation of simple logging for internal query-framework use.
 */

#pragma once

#include <string_view>

namespace query {

	namespace detail {
		constexpr bool LOG_QUERY_EVENTS = false;
	}
	void log(std::string_view str);
}
