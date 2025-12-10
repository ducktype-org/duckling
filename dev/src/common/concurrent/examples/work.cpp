#include "work.hpp"
#include <concurrent/workers/worker_data.hpp>
#include <random>
// #include <mutex>



// void work(HashMap& map, concurrent::WDRef worker_data) {
void work(HashMap& map) {
    u64 key = 21123;

    map.maybePut(key, 0);
    auto new_val = map[key] + 1;
    map[key] = new_val;


}


void cWork(CHashMap& map, concurrent::WDRef worker_data) {
    u64 key = 123123;
    // key %= 256;

    map.tryPut(key, 0);
    auto new_val = map.getCopy(key) + 1;
    map.update(key, new_val);
}


