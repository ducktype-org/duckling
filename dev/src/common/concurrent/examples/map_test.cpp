#include "work.hpp"
#include <thread>

constexpr int THREAD_COUNT = 4;

int main() {
    std::vector<std::jthread> threads;
    threads.reserve(THREAD_COUNT);
    
    // HashMap map;
    CHashMap cmap;

    cmap.put(211231ull, 0ull);


    for (int i = 0; i < THREAD_COUNT; i++) {
        threads.emplace_back([&]() {
            for (u64 j = 0; j < 100'000; j++) {
                // work(map);
                cWork(cmap);
            }
        });
    }

    return 0;
}