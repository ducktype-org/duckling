#pragma once

namespace query {
	/**
	 * Whether the query statistics collection is enabled or not.
	 * This is a compile-time constant for performance reasons.
	 *
	 * @TODO: #2024 Query stats are not thread safe, so they are currently disabled by default.
	 * Change that.
	 */
	constexpr bool USE_STATS = false;
}
