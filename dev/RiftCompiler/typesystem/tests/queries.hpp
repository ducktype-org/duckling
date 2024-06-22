#pragma once

#include <query_framework/query_int.hpp>

namespace ts {

	/**
	 * @brief Query to get the size of a type.
	 *
	 * @note Prefer to use the TypeInfo::getSize method directly for efficiency.
	 * This query is for access through a query::entryPoint.
	 */
	DECLARE_QUERY(QuerySizeOfType, TypeInfo, usize)
}
