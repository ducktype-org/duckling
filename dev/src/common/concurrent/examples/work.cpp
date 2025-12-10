#include "work.hpp"
// #include <mutex>

void work(HashMap& map) {
    u64 key = 211231;
    map.maybePut(key, 0);
    auto new_val = map[key] + 1;
    map[key] = new_val;

    // if (map.contains(key)) {
    //     auto new_val = map[key] + 1;
    //     map[key] = new_val;
    // } else {
    //     map.put(key, 1);
    // }
}



void cWork(CHashMap& map) {
    u64 key = 211231;
    map.tryPut(key, 0);
    auto new_val = map.getCopy(key) + 1;
    map.update(key, new_val);
}


