#include "work.hpp"
#include <mutex>

void work(HashMap& map) {
    u64 key = 211231;
    if (map.contains(key)) {
        auto new_val = map[key] + 1;
        map[key] = new_val;
    } else {
        map.put(key, 1);
    }
}


std::mutex mutex;
u64 counter = 0;

void cWork(CHashMap& map) {
    // std::lock_guard<std::mutex> guard(mutex);
    mutex.lock();


    // u64 key = 211231;
    map.tryPut(211231 + (counter++), 1);
    // if (map.contains(key)) {
    //     // auto new_val = map.getCopy(key) + 1;
    //     // map.update(key, new_val);
    // }
    mutex.unlock();
}


