#include "logs.hpp"

#include <iostream>

namespace query::detail {
	void log(std::string_view str) {
		// @TODO: we should probably have some common module for functions like this,
		// and optional debug logging
		std::cerr << str;
	}
}
