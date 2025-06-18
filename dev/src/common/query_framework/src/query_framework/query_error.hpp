#pragma once

#include "query_result.hpp"  // IWYU pragma: export

namespace query {
	template<class T>
	using QError = detail::errors::QError<T>;
}
