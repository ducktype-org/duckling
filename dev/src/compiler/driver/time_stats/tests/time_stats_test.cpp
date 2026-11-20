// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <time_stats/time_stats.hpp>
#include <timer/timer.hpp>

#include <tester/tester.hpp>

#include <chrono>
#include <thread>

class CompilerTimeStatsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CompilerTimeStatsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(prettyPrintTest);
	}

private:
	void simpleTest() {
		// we start at 0 for all categories:
		for (u64 i = 0; i < std::to_underlying(time_stats::TimeCategories::Sentinel); ++i) {
			ASSERT_EQUAL(
				time_stats::getTimeStatistic(static_cast<time_stats::TimeCategories>(i)).value,
				timer::Duration::zero().value
			);
		}

		{
			time_stats::TrackCategoryTime tracker(time_stats::TimeCategories::DriverInitialization);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		{
			time_stats::TrackCategoryTime tracker(time_stats::TimeCategories::BackendCompilation);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		auto driver_init_time
			= time_stats::getTimeStatistic(time_stats::TimeCategories::DriverInitialization).value;
		auto backend_comp_time
			= time_stats::getTimeStatistic(time_stats::TimeCategories::BackendCompilation).value;

		ASSERT_TRUE(driver_init_time >= std::chrono::milliseconds(100 - 1));
		ASSERT_TRUE(backend_comp_time >= std::chrono::milliseconds(200 - 1));

		{
			time_stats::TrackCategoryTime tracker(time_stats::TimeCategories::BackendCompilation);
			tracker.end();  // explicit end
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		auto backend_comp_time_after
			= time_stats::getTimeStatistic(time_stats::TimeCategories::BackendCompilation).value;

		// we only added time before the explicit end, so the time should be almost the same
		// 20 miliseconds margin to capture cases where we lost cpu in just the right moment
		ASSERT_TRUE(backend_comp_time_after - backend_comp_time < std::chrono::milliseconds(20));
	}

	void prettyPrintTest() {
		// just call the pretty print function to ensure it doesn't crash
		time_stats::prettyPrintTimeStatistics();
	}


public:
	~CompilerTimeStatsTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/driver/time_stats/tests/");
