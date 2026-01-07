#pragma once

#include <timer/timer.hpp>

#include <base/types/ints.hpp>

namespace time_stats {

	/**
	 * Categories of tracked time.
	 * Feel free to extend/modify as needed.
	 *
	 * @important this enum should provide no explicit values, so the sentinel value is always
	 * correct, and as its underlying values are used for indexing time statistics arrays.
	 *
	 * @note When adding new categories also update prettyPrintTimeStatistics()
	 */
	enum class TimeCategories : u64 {

		/**
		 * Total compilation time.
		 * @note Measures in the main.cpp
		 */
		TotalCompilationTime,

		/**
		 * Time spend in Driver initialization functions.
		 * @note TotalCompilationTime does not include this time.
		 */
		DriverInitialization,

		/**
		 * Time spend in Driver exit functions.
		 * @note TotalCompilationTime does not include this time.
		 */
		DriverExit,

		/**
		 * Time spent on PST construction, tracked by the PST.
		 */
		PSTConstruction,

		/**
		 * Time spent on backend compilation.
		 * Used for time statistics collection.
		 * @note it doesn't measure it based on the backend modules,
		 * but based on driver operations.
		 * Any non-driver operations are not measured.
		 * @note it may be, in the future, moved to some other, more generic place.
		 */
		BackendCompilation,


		Sentinel,  ///< Sentinel value used to determine the number of categories.

	};

	/**
	 * RAII-like object to track time spent in a given category.
	 * Time tracking ends when the object is destroyed or when end() is called.
	 */
	struct TrackCategoryTime final {
		TimeCategories         category;
		timer::TimeMeasurement measurement;
		bool                   ended;

		TrackCategoryTime(TimeCategories category);

		/**
		 * Explicitly ends time tracking for this object.
		 * After calling this function, destructor will do nothing.
		 */
		void end();

		~TrackCategoryTime();
	};

	/**
	 * Pretty-prints collected time statistics to std::cerr.
	 */
	void prettyPrintTimeStatistics();
}
