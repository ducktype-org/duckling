#include "q_stats.hpp"
#include <chrono>
#include <base/maps.hpp>
#include <base/ref.hpp>
#include <iostream>

template<>
struct std::hash<query::internal::QueryID> final {
    auto operator()(const query::internal::QueryID& id) const noexcept {
        return std::hash<u64>()(id.asInt());
    }
};

namespace query {

    namespace {
        using TimeStamp = decltype(std::chrono::high_resolution_clock::now());
        using Duration = std::chrono::duration<double, std::milli>; // milliseconds as double

        struct QueryStatData final {
            u64 num_calls = 0;
            u64 num_provide_calls = 0;
            Duration total_call_time {};

            TimeStamp last_call_time {};
        };
        
        base::HashMap<internal::QueryID, QueryStatData> data;

        Ref<QueryStatData> getQueryStatData(internal::QueryID query_id) {
         if (!data.contains(query_id)) {
                data.put(query_id, {});
            }
            return &data.at(query_id);
        }
    }



    CallStatsObject::CallStatsObject(internal::QueryID query_id): query_id(query_id) {
        Ref data_ref = getQueryStatData(query_id);

        data_ref->num_calls++;
        data_ref->last_call_time = std::chrono::high_resolution_clock::now();
    }

    CallStatsObject::~CallStatsObject() {
        auto now = std::chrono::high_resolution_clock::now();

        Ref data_ref = getQueryStatData(query_id);
        auto duration = std::chrono::duration_cast<Duration>(now - data_ref->last_call_time);

        if (was_provide_call) {
            data_ref->num_provide_calls += 1;
        }

        data_ref->total_call_time += duration;
    }

    void printStats() {
        for (const auto& [query_id, stat_data] : data) {
            std::cerr << "Query ID: " << query_id.getData().name << "\n";
            std::cerr << "    Number of Calls:   " << stat_data.num_calls << "\n";
            std::cerr << "    Number of P-Calls: " << stat_data.num_provide_calls << "\n";
            std::cerr << "    Total Call Time:   " << stat_data.total_call_time.count() << "ms\n";
            std::cerr << "\n";
        }
    }
}


