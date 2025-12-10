#include "work.hpp"
#include <thread>

constexpr int THREAD_COUNT = 4;

int main() {
    // HashMap map;
    CHashMap cmap;


    std::vector<std::jthread> threads;
    threads.reserve(THREAD_COUNT);
    

    cmap.put(211231ull, 0ull);


    for (int i = 0; i < THREAD_COUNT; i++) {
        threads.emplace_back([&]() {
            for (u64 j = 0; j < 1'000; j++) {
                // work(map);
                cWork(cmap);
            }
        });
    }

    // for (auto& thread : threads) {
    //     thread.join();
    // }

    return 0;
}