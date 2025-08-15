#include "q_stats.hpp"
#include <chrono>
#include <base/maps.hpp>
#include <base/ref.hpp>

namespace query {

    namespace {
        using TimeStamp = decltype(std::chrono::high_resolution_clock::now());
        using TimeSum   = std::chrono::duration_cast<std::chrono::nanoseconds>(1s);

        struct QueryStatData final {
            u64 num_calls = 0;
            TimeSum total_call_time {};

            TimeStamp last_call_time {};
        };
        
        base::HashMap<internal::QueryID, QueryStatData> data;
    }

    CallStatsObject::CallStatsObject(internal::QueryID query_id) {
        if (not data.contains(query_id)) {
            data.put(query_id, {});
        }
        Ref data_ref = &data.at(query_id);

        data_ref->num_calls++;
        data_ref->last_call_time = std::chrono::high_resolution_clock::now();
    }

    CallStatsObject::~CallStatsObject() {
        // Implementation for stopping the statistics collection
    }

} // namespace query