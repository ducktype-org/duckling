#pragma once

#include "base/maps.hpp"
#include "query_id.hpp"

namespace query::detail {

	/**
	 * @brief A simple counter for providing unique query id-s.
	 * This function should never be used outside the framework.
	 * @param pretty_name A name for the new query.
	 */
	QueryID newQueryId(const std::string& pretty_name);

	/**
	 * @brief Provides query id of "outside world" query.
	 * This function should never be used outside the framework.
	 */
	QueryID outsideWorldQueryID();
}
