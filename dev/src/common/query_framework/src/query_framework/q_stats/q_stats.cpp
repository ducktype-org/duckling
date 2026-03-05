#include "q_stats.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <timer/timer.hpp>

#include <base/pointers/ref.hpp>

#include <iostream>

namespace query {

	timer::AtomicDuration total_red_green_sweep_time = timer::Duration::zero();
	timer::AtomicDuration total_graph_merge_time     = timer::Duration::zero();

	namespace {

		/**
		 * Tag used to emplace construct QueryStatData in the ConHashMap.
		 * @TODO: #2056 Remove this when ConHashMap supports proper in-place construction.
		 */
		struct QueryStatsDataConstructionTag final {};

		/**
		 * Data structure to hold statistics for a single query (not query call).
		 */
		struct QueryStatsData final {
			std::atomic<u64>      num_calls         = 0;
			std::atomic<u64>      num_provide_calls = 0;
			timer::AtomicDuration total_call_time   = timer::Duration::zero();

			QueryStatsData(QueryStatsDataConstructionTag) {}
		};

		/**
		 * Map from QueryID to its statistics data.
		 */
		concurrent::ConHashMap<internal::QueryID, QueryStatsData> data;

		/**
		 * Get the statistics data for a specific query.
		 */
		Ref<QueryStatsData> getQueryStatData(internal::QueryID query_id) {
			auto maybe_inserted = data.maybePut(query_id, QueryStatsDataConstructionTag{});
			if (maybe_inserted) return &maybe_inserted->value;
			return data.at(query_id);
		}
	}

	CallStatsObject::CallStatsObject(internal::QueryID query_id): query_id(query_id) {
		Ref data_ref = getQueryStatData(query_id);

		data_ref->num_calls.fetch_add(1, std::memory_order_relaxed);
		this->call_time.reset();
		this->call_time.startMeasurement();
	}

	CallStatsObject::~CallStatsObject() {
		this->call_time.endMeasurement();

		Ref data_ref = getQueryStatData(query_id);

		if (was_provide_call) data_ref->num_provide_calls.fetch_add(1, std::memory_order_relaxed);

		data_ref->total_call_time.add(this->call_time.duration());
	}

	void printStats() {
		std::cerr << "=== Query Framework Per Query Statistics ===\n\n";
		for (const auto& [query_id, stat_data]: data) {
			std::cerr << "Query ID: " << query_id.getData().name << "\n";
			std::cerr << "    Number of Calls:   "
					  << stat_data.num_calls.load(std::memory_order_relaxed) << "\n";
			std::cerr << "    Number of P-Calls: "
					  << stat_data.num_provide_calls.load(std::memory_order_relaxed) << "\n";
			std::cerr << "    Total Call Time:   ";
			timer::printAs(
				std::cerr, stat_data.total_call_time.toDuration(), timer::TimeUnit::Milliseconds
			);
			std::cerr << "\n\n";
		}

		std::cerr << "=== Query Framework Other Statistics ===\n\n";
		std::cerr << "Total time spent in red-green sweeps: ";
		timer::printAs(
			std::cerr, total_red_green_sweep_time.toDuration(), timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Total time spent in graph merges: ";
		timer::printAs(
			std::cerr, total_graph_merge_time.toDuration(), timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n\n";
	}
}
