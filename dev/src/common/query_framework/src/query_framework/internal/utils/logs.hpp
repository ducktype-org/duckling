/**
 * @file logs.hpp
 * @brief Implementation of simple logging for internal query-framework use.
 */

#pragma once

#include <base/str/str_utils.hpp>  // IWYU pragma: export

#include <string_view>

namespace query::internal {
	constexpr bool LOG_QUERY_EVENTS = false;
	void           log(std::string_view str);
}

#define QUERY_DEBUG_LOG(...)                                \
	if constexpr (query::internal::LOG_QUERY_EVENTS) {      \
		query::internal::log(base::strConcat(__VA_ARGS__)); \
	}
