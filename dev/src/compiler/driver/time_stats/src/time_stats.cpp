#include "time_stats.hpp"

#include <utility>
#include <array>
#include <timer/timer.hpp>

namespace time_stats {

    namespace {
        constexpr u64 TIME_CATEGORIES_COUNT = std::to_underlying(TimeCategories::Sentinel);
        
        constinit std::array<timer::Duration, TIME_CATEGORIES_COUNT> time_statistics{};
        constinit std::array<bool, TIME_CATEGORIES_COUNT> is_category_active{};
    }


    TrackCategoryTime::TrackCategoryTime(TimeCategories category):
        category(category),
        add_to_time_object(
            &time_statistics.at(std::to_underlying(category))
        ) 
    {
        CORE_ASSERT(
            not is_category_active.at(std::to_underlying(category)),
            "Overlapping time tracking of category ", std::to_underlying(category), "."
        );
        is_category_active.at(std::to_underlying(category)) = true;
    }

    TrackCategoryTime::~TrackCategoryTime() {
        CORE_ASSERT_NOEXCEPT(
            is_category_active.at(std::to_underlying(category)),
            "Ending time tracking of inactive category ", std::to_underlying(category), "."
        );
        is_category_active.at(std::to_underlying(category)) = false;
       
        // note that timer::AddToTime destructor is called after this,
        // so time will be automatically added to the statistics
    }
}

