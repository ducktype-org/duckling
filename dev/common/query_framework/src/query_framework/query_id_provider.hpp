#pragma once

#include "query_id.hpp"

namespace query::detail {

	/**
	 * @brief A simple counter for providing unique query id-s.
	 * This function should never be used outside the framework.
	 */
	QueryID nextQueryId();

	/**
	 * @brief Provides query id of "outside world" query.
	 * This function should never be used outside the framework.
	 */
	QueryID outsideWorldQueryID();
}
