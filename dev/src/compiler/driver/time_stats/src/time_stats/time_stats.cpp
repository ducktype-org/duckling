#include "time_stats.hpp"

#include <time_stats/module_flags/module_flags.hpp>
#include <timer/timer.hpp>

#include <array>
#include <atomic>
#include <iostream>
#include <utility>

namespace time_stats {

	namespace {
		constexpr u64 TIME_CATEGORIES_COUNT = std::to_underlying(TimeCategories::Sentinel);

		/**
		 * Following arrays are used to store time statistics and active status
		 * of each category.
		 *
		 * \parallel This need to stay thread-safe.
		 */
		constinit std::array<timer::AtomicDuration, TIME_CATEGORIES_COUNT> time_statistics{};

		/**
		 * @note This is intentionally thread local, as it is valid for each thread to track the
		 * same category at the same time.
		 */
		constinit thread_local std::array<bool, TIME_CATEGORIES_COUNT> is_category_active{};
	}

	TrackCategoryTime::TrackCategoryTime(TimeCategories category):
		  category(category),
		  ended(false) {
		if (not ENABLE_TIME_STATS) return;  // intentionally do nothing.

		bool was_active = is_category_active.at(std::to_underlying(category));
		is_category_active.at(std::to_underlying(category)) = true;

		CORE_ASSERT(
			not was_active,
			"Overlapping time tracking of category ",
			std::to_underlying(category),
			"."
		);
		measurement.startMeasurement();
	}

	void TrackCategoryTime::end() {
		if (not ENABLE_TIME_STATS) return;  // intentionally do nothing.

		// multiple calls to end() do nothing:
		if (ended) return;

		measurement.endMeasurement();

		bool was_active = is_category_active.at(std::to_underlying(category));
		is_category_active.at(std::to_underlying(category)) = false;

		CORE_ASSERT(
			was_active,
			"Ending time tracking of inactive category ",
			std::to_underlying(category),
			"."
		);

		ended = true;

		time_statistics.at(std::to_underlying(category)).add(measurement.duration());
	}

	TrackCategoryTime::~TrackCategoryTime() {
		if (not ENABLE_TIME_STATS) return;  // intentionally do nothing.

		// we don't do anything if already ended:
		if (ended) return;

		measurement.endMeasurement();

		bool was_active = is_category_active.at(std::to_underlying(category));
		is_category_active.at(std::to_underlying(category)) = false;

		CORE_ASSERT_NOEXCEPT(
			was_active,
			"Ending time tracking of inactive category ",
			std::to_underlying(category),
			"."
		);

		time_statistics.at(std::to_underlying(category)).add(measurement.duration());
	}

	timer::Duration getTimeStatistic(TimeCategories category) {
		return time_statistics.at(std::to_underlying(category)).toDuration();
	}

	void prettyPrintTimeStatistics() {
		std::cerr << "=== Time statistics collected by compiler time_stats module ===\n\n";

		if (not ENABLE_TIME_STATS) {
			std::cerr << "Time statistics collection is disabled. No statistics to show.\n";
			return;
		}

		std::cerr << "Driver initialization time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::DriverInitialization)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Driver exit time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::DriverExit)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Graph optimization time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::GraphOptimization)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Total compilation time (note that subcategories may overlap): ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::TotalCompilationTime)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - PST construction time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::PSTConstruction)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - Backend compilation time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::BackendCompilation)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - Linking time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::Linking)).toDuration(),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n\n";
	}
}
