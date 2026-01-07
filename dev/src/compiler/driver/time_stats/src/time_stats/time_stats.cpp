#include "time_stats.hpp"

#include <timer/timer.hpp>

#include <array>
#include <iostream>
#include <utility>

namespace time_stats {

	namespace {
		constexpr u64 TIME_CATEGORIES_COUNT = std::to_underlying(TimeCategories::Sentinel);

		/**
		 * Following arrays are used to store time statistics and active status
		 * of each category.
		 *
		 * \parallel They will have to be made thead-safe if time tracking from multiple threads
		 * is to be supported (perhaps via thread-local storage).
		 */
		constinit std::array<timer::Duration, TIME_CATEGORIES_COUNT> time_statistics{};
		constinit std::array<bool, TIME_CATEGORIES_COUNT>            is_category_active{};
	}

	TrackCategoryTime::TrackCategoryTime(TimeCategories category):
		  category(category),
		  ended(false) {
		CORE_ASSERT(
			not is_category_active.at(std::to_underlying(category)),
			"Overlapping time tracking of category ",
			std::to_underlying(category),
			"."
		);
		is_category_active.at(std::to_underlying(category)) = true;
		measurement.startMeasurement();
	}

	void TrackCategoryTime::end() {
		// multiple calls to end() do nothing:
		if (ended) return;

		measurement.endMeasurement();

		CORE_ASSERT(
			is_category_active.at(std::to_underlying(category)),
			"Ending time tracking of inactive category ",
			std::to_underlying(category),
			"."
		);
		is_category_active.at(std::to_underlying(category)) = false;
		ended                                               = true;

		time_statistics.at(std::to_underlying(category)).value += measurement.duration().value;
	}

	TrackCategoryTime::~TrackCategoryTime() {
		// we don't do anything if already ended:
		if (ended) return;

		measurement.endMeasurement();

		CORE_ASSERT_NOEXCEPT(
			is_category_active.at(std::to_underlying(category)),
			"Ending time tracking of inactive category ",
			std::to_underlying(category),
			"."
		);
		is_category_active.at(std::to_underlying(category)) = false;

		time_statistics.at(std::to_underlying(category)).value += measurement.duration().value;
	}

	void prettyPrintTimeStatistics() {
		std::cerr << "=== Time statistics collected by compiler time_stats module ===\n\n";

		std::cerr << "Driver initialization time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::DriverInitialization)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Driver exit time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::DriverExit)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << "Total compilation time (note that subcategories may overlap): ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::TotalCompilationTime)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - PST construction time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::PSTConstruction)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - Backend compilation time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::BackendCompilation)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n";

		std::cerr << " - Linking time: ";
		timer::printAs(
			std::cerr,
			time_statistics.at(std::to_underlying(TimeCategories::Linking)),
			timer::TimeUnit::Milliseconds
		);
		std::cerr << "\n\n";
	}
}
