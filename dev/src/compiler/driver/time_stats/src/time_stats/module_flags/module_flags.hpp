#pragma once

namespace time_stats {
	/**
	 * If set, time_stats module will collect and output time statistics.
	 * If unset, time_stats module will do nothing, and the collected time statistics will be empty.
	 *
	 * @note It is needed mostly, because time statistics collection can have significant
	 * performance overhead, and we don't want to pay it when we don't need it.
	 *
	 * PR: no diff
	 */
	constexpr bool ENABLE_TIME_STATS = false;
}
