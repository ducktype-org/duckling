/**
 * @file logs.hpp
 * @brief Implementation of simple logging for internal query-framework use.
 */

#pragma once

#include <string_view>

namespace query {

	void log(std::string_view str);

}
