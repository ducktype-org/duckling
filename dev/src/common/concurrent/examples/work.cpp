#include "work.hpp"
#include <concurrent/workers/worker_data.hpp>
#include <random>
// #include <mutex>



void work(HashMap& map, concurrent::WDRef worker_data) {
    u64 key = 123123 + (worker_data->rng() % 1024);

    map.maybePut(key, 0);
    auto new_val = map[key] + 1;
    map[key] = new_val;


}


void cWork(CHashMap& map, concurrent::WDRef worker_data) {
    u64 key = 123123 + (worker_data->rng() % 4096);

    map.tryPut(key, 0);
    auto new_val = map.getCopy(key) + 1;
    map.update(key, new_val);
}


void gtlWork(GTLMap& map, concurrent::WDRef worker_data) {
    u64 key = 123123 + (worker_data->rng() % 4096);

    map.insert_or_assign(key, 0);
    
    auto new_val = map[key] + 1;
    map.insert_or_assign(key, new_val);


}

