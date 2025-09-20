#pragma once

#include <query_framework/internal/query_data/query_id.hpp>

namespace query {
	/**
	 *  Whether the query statistics collection is enabled or not.
	 *  @TODO #1058: Change it to proper, runtime config
	 */
	constexpr bool USE_STATS = true;

	/**
	 * RAII-like object to collect statistics about query calls.
	 * Should be used in a way that encapsulates the entire call to a query function.
	 */
	struct CallStatsObject final {
		internal::QueryID query_id;
		bool              was_provide_call = false;

		/**
		 * Call when query call starts
		 */
		CallStatsObject(internal::QueryID query_id);

		/**
		 * Call when query call ends
		 */
		~CallStatsObject();
	};

	struct NoStats final {
		NoStats(internal::QueryID) {}
	};

	/**
	 * Print collected statistics to the cerr.
	 */
	void printStats();
}
