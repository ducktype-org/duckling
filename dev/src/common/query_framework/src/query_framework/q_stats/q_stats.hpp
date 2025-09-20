#pragma once

#include <query_framework/internal/query_data/query_id.hpp>
#include <timer/timer.hpp> // @TODO #404: relax it, so query does not leak timer

namespace query {
	/**
	 *  Whether the query statistics collection is enabled or not.
	 *  @TODO #1058: Change it to proper runtime/comptime config value
	 */
	constexpr bool USE_STATS = true;

	/**
	 * RAII-like object to collect statistics about query calls.
	 * Should be used in a way that encapsulates the entire call to a query function.
	 */
	struct CallStatsObject final {
		internal::QueryID query_id;
		bool              was_provide_call = false;

        timer::TimeMeasurement call_time = {};

		/**
		 * Call when query call starts
		 */
		CallStatsObject(internal::QueryID query_id);

		/**
		 * Call when query call ends
		 */
		~CallStatsObject();
	};

	/**
	 * Mock struct used in query implementations when statistics are disabled.
	 */
	struct NoStats final {
		NoStats(internal::QueryID) {}

		bool was_provide_call = false;
	};

	/**
	 * Print collected statistics to the cerr.
	 */
	void printStats();
}
