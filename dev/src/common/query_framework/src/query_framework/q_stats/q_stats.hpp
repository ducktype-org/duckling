/**
 * Various statistics collected during query framework operation.
 * Statistics include time spent in queries, number of cache hits/misses, time spend in red-green
 * sweeps etc.
 */

#pragma once

#include <timer/timer.hpp>  // @TODO #404: relax it, so query does not leak timer

#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/module_flags/module_flags.hpp>  // IWYU pragma: export (for USE_STATS)

namespace query {

	/**
	 * Total time spent in red-green sweeps across all queries.
	 */
	extern timer::AtomicDuration total_red_green_sweep_time;

	/**
	 * Total time spent in graph merges across all queries.
	 */
	extern timer::AtomicDuration total_graph_merge_time;

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
