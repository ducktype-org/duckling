#pragma once

#include <base/types/ints.hpp>
#include <timer/timer.hpp>

namespace time_stats {

    /**
	 * Categories of tracked time.
	 * Feel free to extend/modify as needed.
     *
     * @important this enum should provide no explicit values, so the sentinel value is always correct,
     * and as its underlying values are used for indexing time statistics arrays.
	 */
    enum class TimeCategories: u64 {
        /**
        * Time spent on backend compilation.
        * Used for time statistics collection.
        * @note it doesn't measure it based on the backend modules,
        * but based on driver operations.
        * Any non-driver operations are not measured.
        * @note it may be, in the future, moved to some other, more generic place.
        */
        BackendCompilation,


        Sentinel, ///< Sentinel value used to determine the number of categories.
        
    };

    // namespace internal {

    // }

    /** 
     * RAII-like object to track time spent in a given category.
     */
    struct TrackCategoryTime final {
        TimeCategories category;
        timer::AddToTime add_to_time_object;

        TrackCategoryTime(TimeCategories category);

        ~TrackCategoryTime();
    };

}

