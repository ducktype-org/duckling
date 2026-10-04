#pragma once

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

namespace query {
	/**
	 * @brief The Failed class is used as a marker to indicate that a query has failed.
	 * It is returned by queries in QResult.
	 *
	 * The intended semantics of this type is as follows:
	 *
	 * - When a query returns a query::Failed state, it indicates that the query could not
	 *   successfully compute a result, and that all relevant diagnostic reporting has already been
	 * performed.
	 *
	 * - Queries that depend on other queries should generally propagate the Failed state if any of
	 * their dependencies return Failed. This allows Failed state to cascade naturally through the
	 * query graph, but diagnostic to be reported only once at the source of the failure.
	 *
	 * @note This class is intentionally empty.
	 */
	class Failed final {};

	/**
	 * A simple wrapper to throw QueryFailedException that can be caught by the query framework.
	 */
	[[noreturn]] void throwFailed(std::string_view reason = "Query failure");
}
