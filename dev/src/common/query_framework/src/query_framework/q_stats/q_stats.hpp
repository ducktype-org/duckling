#pragma once

#include <timer/timer.hpp>  // @TODO #404: relax it, so query does not leak timer

#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/configuration/configuration.hpp> // IWYU pragma: export (for USE_STATS)

namespace query {

	/**
	 * RAII-like object to collect statistics about a single query call.
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
