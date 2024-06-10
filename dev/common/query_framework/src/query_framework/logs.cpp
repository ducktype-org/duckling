#include "logs.hpp"

#include <iostream>

namespace query {
	// @TODO: generally if logging is disabled then calculation of log arguments should not happen
	constexpr bool LOG_QUERY_EVENTS = true;

	void log(std::string_view str) {
		if constexpr (LOG_QUERY_EVENTS) std::cerr << str;
	}
}
