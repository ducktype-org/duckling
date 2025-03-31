/**
 * @file logs.hpp
 * @brief Implementation of simple logging for internal query-framework use.
 */

#pragma once

#include <string_view>

#include <base/str_utils.hpp>  // IWYU pragma: export

namespace query::detail {
	constexpr bool LOG_QUERY_EVENTS = false;
	void           log(std::string_view str);
}

#define QUERY_DEBUG_LOG(...)                              \
	if constexpr (query::detail::LOG_QUERY_EVENTS) {      \
		query::detail::log(base::strConcat(__VA_ARGS__)); \
	}
